# Crash lab answers

Each answer covers the **diagnosis path** (the commands that get you there),
the **root cause**, the **fix**, and **where the same bug can hide in Gringofts**.
The diagnosis path matters more than the fix.

---

## 01 — null deref
**Path:** `run_lab.sh 01` → `gdb lab01 core` → `bt`:
```
#0 StateMachine::apply (cmd=...)       <- crash site: cmd.delta() reads the vptr through a null reference
#1 replayOne (entry=...)               <- bug site
#2 recover ...
```
`frame 1`, `print cmd` → `std::unique_ptr<Command> = {get() = 0x0}`; `print entry` → `{index = 4, type = 2, ...}`.
The warning `Unknown command type: 2` just before the crash confirms it.

**Root cause:** `decodeCommand` returns `nullptr` for unknown types. The caller dereferences it
without checking. The crash shows up one frame later, inside `apply`, because a reference was
formed from a null pointer (`*cmd`). That is already undefined behaviour, but nothing faults until memory is actually read.

**Fix:** check for null and decide on a policy. For recovery, silently skipping entries would
corrupt state (replicas diverge), so fail fast with a clear message:
```cpp
auto cmd = decodeCommand(entry);
if (!cmd) {
  LOG_ERROR("cannot decode entry %lu type %u: binary too old for this log?", entry.index, entry.type);
  std::abort();   // or throw; never skip
}
```
**In Gringofts:** `CommandDecoderImpl`/`EventDecoderImpl` return `nullptr` for unknown types.
Every caller must handle that. It happens when a newer version wrote the log and an older binary
replays it (a rollback during a rolling upgrade).

## 02 — uncaught exception in a thread
**Path:** the message `terminate called after throwing an instance of 'QueueStoppedException'`
names the exception. In gdb, `bt` shows `std::terminate ← __cxa_throw ← BlockingQueue::dequeue ← CommandProcessLoop::run ← lambda`
on the **loop thread**, not main. Live alternative: `gdb --args lab02`, `catch throw`, `run`: it stops at the throw.

**Root cause:** the shutdown contract says `dequeue()` throws once the queue is stopped. The loop
doesn't catch it. An exception that escapes a `std::thread`'s function calls `std::terminate()`.
(In Java the thread would just die and log a stack trace.) The `mRunning` flag doesn't help: the
thread is blocked inside `dequeue()`, not at the top of the loop.

**Fix:**
```cpp
void run() {
  try {
    while (mRunning) { process(mQueue->dequeue()); }
  } catch (const QueueStoppedException &) {
    LOG_INFO("queue stopped, loop exiting");
  }
}
```
Also wrap the bodies of long-lived threads in `catch (const std::exception &e)` that logs and then
aborts deliberately, so a surprise exception leaves a useful log line.
**In Gringofts:** every consumer of `MpscQueue::dequeue()` / `BlockingQueue` (for example `CommandProcessLoop`) must follow this contract.

## 03 — constructor throws after starting a thread
**Path:** core + `bt` → `std::terminate ← std::thread::~thread ← App::App ← main`. The `terminate`
is called from the destructor of the **member** `mHeartbeatThread`, during unwinding out of the constructor.

**Root cause:** if a constructor throws, **the object's destructor never runs**, because the object never
existed. Members that were already constructed are destroyed, and a joinable `std::thread` member
calls `std::terminate()` in its destructor. So `~App()`, with its careful join, never executes.
The exception never reaches `main`'s `catch`, so the error is never logged. The same thing happens
with a bad port like `abc` (`std::invalid_argument`) and an out-of-range one like `99999`.

**Fix (order):** validate first and start threads last: `mPort = parsePort(port);` before creating the thread.
**Fix (robust):** give the thread its own RAII owner that joins in its destructor (C++20's
`std::jthread` does this; in C++17 write a tiny `JoiningThread` wrapper, like ex 10's `Worker`),
or start threads in an explicit `start()` method rather than the constructor.
**In Gringofts:** `App` owns several `std::thread` members. Any throw in startup (a bad INI value, a
port already in use) after one of them has started can turn a clean error into a core dump.

## 04 — pure virtual method called
**Path:** `bt` on the crashing thread: `std::terminate ← __cxa_pure_virtual ← Loop::start()::lambda`.
The main thread is inside `Loop::~Loop` → `drain()`.

**Root cause:** destruction runs from the most-derived class to the base: `~EventApplyLoop()` finishes, then the
vptr is reset to `Loop`'s, then `~Loop()` runs. The thread is still running during `drain()` and calls
`runOnce()`, which is now pure virtual. (Before the vptr is reset, it would be calling into a
destroyed derived object instead, which is worse.) Joining in the base destructor is **too late**.

**Fix:** stop the thread before any part of the object is destroyed. Either call `stop()` from
the most-derived destructor, or better, make the owner call `loop.stop()` explicitly before
destruction (Gringofts' `App::shutdown()` does this) and have `~Loop()` only
`assert(!mThread.joinable())`. Rule: **never let a thread outlive the parts of the object it calls into.**

## 05 — two owners
**Path:** glibc: `double free or corruption (out)`. ASan: `attempting free on address which was not malloc()-ed`,
with the stack showing `InflightTracker::remove → ~shared_ptr → delete`.

**Root cause:** `std::shared_ptr<Command>(this)` creates a **second, independent control block**.
When the tracker's copy drops to zero, it `delete`s an object that is actually embedded in
`make_shared`'s single allocation (hence "not malloc()-ed"), and the original owner will delete it again.

**Fix:** `class Command : public std::enable_shared_from_this<Command>` and `tracker->add(mId, shared_from_this());`.
(Only valid when the object is already owned by a `shared_ptr`.)

## 06 — dangling reference into a vector
**Path:** ASan build: `heap-use-after-free ... READ of size 8` at `main.cpp:37`. The "freed by" stack
goes through `std::vector<Peer>::_M_realloc_insert` ← `ClusterInfo::addPeer`. The "allocated by" stack
shows the vector's original buffer.

**Root cause:** `push_back` reallocated the vector. Every reference, pointer and iterator into the old
buffer now dangles. Other invalidating operations: `insert`/`erase`/`resize`/`reserve` on `vector`, any
rehash of `unordered_map`/`unordered_set`, `std::string` growth, `deque` insert at the ends (invalidates iterators).
(`std::map`/`std::list` node references survive insertions.)

**Fix:** store the leader **id** and look it up when needed, or copy the `Peer` (`Peer leader = cluster.peer(0);`).
Returning `const T&` from accessors is fine; *storing* it across mutations is not.

**Aside:** in an earlier version of this lab the peer was a bare `std::string`, and ASan reported only
`SEGV in strlen`, not use-after-free. The reads happened inside libstdc++, which isn't instrumented.
ASan sees only what was compiled with `-fsanitize=address`.

## 07 — heap overflow
**Path:** a core's `bt` shows `malloc_printerr ← _int_malloc ← operator new ← std::string` in `main`: innocent.
ASan: `heap-buffer-overflow ... WRITE of size 24` in `encode` at the `memcpy` of the header, with
"0 bytes after 17-byte region allocated in encode".

**Root cause:** `kHeaderSize` was not updated when v2 added two `uint64_t` fields. The buffer is
16 bytes short, so the `memcpy` overwrites the next heap chunk's metadata. glibc only notices on a
**later** `malloc`/`free`, in unrelated code.

**Fix:** derive sizes from types: `new char[sizeof(FrameHeader) + payload.size()]`, or better,
`std::vector<char> buf(sizeof(FrameHeader) + payload.size())` with `static_assert(sizeof(FrameHeader) == 24)`.
**Lesson:** a glibc `malloc(): ...`/`free(): ...` abort means "memory was corrupted *earlier*". Go straight to ASan.
**In Gringofts:** anywhere bytes are packed by hand (crypto, segment files in `raft/storage`, snapshot code).

## 08 — lock-order deadlock
**Path:** `run_lab.sh 08` → gdb on the gcore → `thread apply all bt`:
thread A is in `applyNext` at line 20 (holds log, wants state); thread B is in `takeSnapshot` at line 29
(holds state, wants log). In a frame of each: `print mStateMutex` / `print mLogMutex` → `__owner = <LWP>`.
Each owner is the other thread: a cycle.

**Fix:** a single global order (always log, then state), or acquire both at once with
`std::scoped_lock lock(mLogMutex, mStateMutex);` (deadlock-avoiding). Better still, don't hold
one lock while doing slow work and then take another.
**Prevention:** TSan also reports lock-order inversions (`lock-order-inversion (potential deadlock)`)
even on runs that happen not to deadlock.

## 09 — non-reentrant mutex
**Path:** attach, `bt` → `becomeLeader → term() → std::mutex::lock → futex_wait`. The only thread
is waiting on a mutex **it already holds**.
Clue in the log: the last line is **cut off right after its prefix**
(`[... becomeLeader] [info] [thread N]` with no message). The prefix was written, then the thread blocked
while evaluating the arguments (`term()`) for the rest of the line. A truncated last log line in a hung process tells you where to look.

**Root cause:** `std::mutex` is not reentrant. Java's `synchronized` and `ReentrantLock` are.
Locking twice from the same thread is undefined behaviour; in practice it deadlocks.

**Fix:** keep the lock at the public API only, and have private helpers that assume it's held:
```cpp
uint64_t term() const { std::lock_guard<std::mutex> l(mMutex); return termLocked(); }
uint64_t termLocked() const { return mTerm; }   // caller holds mMutex
```
and in `becomeLeader`, log using `mTerm` / `termLocked()`. Avoid `std::recursive_mutex`: it hides unclear lock ownership.

## 10 — data race
**Path:** repeated normal runs give different counts (`peers=65`, `heartbeats=78888`) or crash inside
`_Hashtable`. TSan: `WARNING: ThreadSanitizer: data race` with two stacks through
`unordered_map::operator[]` from `PeerTable::heartbeat` on two different threads.

**Root cause:** `std::` containers are not thread-safe for concurrent writes (or a write concurrent
with reads). Rehashing while another thread inserts corrupts the table.

**Fix:** a `std::mutex` in `PeerTable` guarding all accessors (a `mutable` mutex for the `const` ones).
`std::atomic` doesn't help: the map *structure* changes, not just a counter. For a fixed peer set,
you could pre-populate the map and make the values `std::atomic<uint64_t>`, so structure never changes.

## 11 — stack overflow
**Path:** `bt 4` shows `applyEvent → apply → applyEvent → apply ...`; `bt -3` shows the entry from
`main`. The frame numbers (~150,000) and `this=<error reading variable>` at the top mean the stack is exhausted.

**Root cause:** overload resolution uses the **static** type. Inside `applyEvent`, `event` is a
`const Event &`, so `apply(event)` picks `apply(const Event &)`, the fallback, which calls
`applyEvent` again. The `ProcessedEvent` overload is never considered.

**Fix:** downcast after checking the type: `return apply(static_cast<const ProcessedEvent &>(event));`
(or `dynamic_cast`, which Gringofts' `AppStateMachine` uses in its registered appliers). And delete
the recursive fallback.

## 12 — shared_ptr cycle
**Path:** the log shows `inflight=0` while `alive Commands` and RSS grow linearly. ASan/LSan at exit:
`detected memory leaks ... 211250000 byte(s) leaked` with the allocation at `Command::Command` (the payload).

**Root cause:** the callback stored *inside* the `Command` captures a `shared_ptr` to that same
`Command`, a reference cycle: `Command → mOnPersisted → lambda → shared_ptr<Command>`. Reference
counting can't collect cycles; Java's tracing GC can.

**Fix:** capture a `std::weak_ptr` (`[weak = std::weak_ptr<Command>(cmd), &replies] { if (auto c = weak.lock()) ... }`),
capture only the data needed (`[id = cmd->id(), &replies]`), or clear `mOnPersisted` after calling it.
**Ops note:** exit 137 = 128 + 9 (SIGKILL). OOM kills leave no core; check `dmesg` / `kubectl describe pod` for `OOMKilled`.

## 13 — erase while iterating
**Path:** build with the libstdc++ debug mode:
```bash
g++ -std=c++17 -g -D_GLIBCXX_DEBUG -Icrash_lab/common crash_lab/13_iterator_invalidation/main.cpp -o lab13d && ./lab13d
```
→ `Error: attempt to increment a singular iterator.` ASan catches the vector loop
(`heap-buffer-overflow` in `dropSlowPeers`); it misses the map loop, because `_Rb_tree_increment` lives
in uninstrumented libstdc++ and ASan's quarantine keeps the freed node readable.

**Root cause:** `erase(it)` invalidates `it`, and the loop then increments it. For the vector,
erasing also shifts elements, so the element after an erased one is skipped, and `end()` moves.

**Fix:**
```cpp
for (auto it = sessions->begin(); it != sessions->end();) {
  if (expired(it->second)) it = sessions->erase(it); else ++it;
}
latencies->erase(std::remove_if(latencies->begin(), latencies->end(),
                                [maxMs](uint64_t l) { return l > maxMs; }), latencies->end());
```
**Tip:** add a `-D_GLIBCXX_DEBUG` build to your test runs. It catches many STL misuses with clear messages.

## 14 — uninitialised member
**Path:** `valgrind --track-origins=yes build/crash_lab/lab14` → `Conditional jump or move depends on
uninitialised value(s) at main (main.cpp:38)`, `Uninitialised value was created by a heap allocation`.

**Root cause:** `bool mIsLeader;` has no initialiser and the constructor doesn't set it. Heap memory
holds whatever was there before (here `0xAB` bytes from earlier allocations), so any non-zero byte reads as `true`.
A Debug build, a different allocation pattern, or a log line changes what's in that memory, so the bug appears and disappears.

**Fix:** `bool mIsLeader = false;`. Give **every** member a default member initialiser.
**Catch it earlier:** clang-tidy `cppcoreguidelines-pro-type-member-init`; `-Wall -Wextra` catches
some cases (locals more than members); MemorySanitizer (clang only) catches it at run time.

## 15 — optimised + stripped
**Path:** `gdb lab15_stripped core` → `bt` shows only `?? ()`. Inside gdb:
`symbol-file build/crash_lab/lab15.debug` → `bt` gives `Ledger::transfer (from=1, to=7, amount=100) at main.cpp:30`.
`info locals` → `src = <optimized out>`, `dst = 0x0`.

**Root cause:** `find(7)` returned `nullptr` for a nonexistent account. `transfer` dereferences `dst`.
`<optimized out>` means the variable's value isn't kept anywhere at that instruction. Recover it
from what *is* there: `info registers`, `disassemble` around `$pc`, or arguments one frame up.

**Fix (code):** validate both lookups and return an error `ProcessHint`/status.
**Fix (process):** see `DEBUGGING.md` → "Make production crashes debuggable".

## 16 — crash handler + symbolising
**Path:** `lab16 2>&1 | crash_lab/scripts/symbolize.sh` →
`ReplicationTracker::onAppendEntriesReply(...) at .../16_crash_handler/main.cpp:30`.
Frame order: the handler itself (`onFatalSignal`), then libc's signal trampoline (`libc.so.6(+0x45330)`),
then **the real crash**, then its callers.

**By hand:**
```bash
nm build/crash_lab/lab16 | grep onAppendEntriesReply        # e.g. 0000000000005ba2 W _ZN18Repl...
addr2line -f -C -i -e build/crash_lab/lab16 0x$(printf '%x' $((0x5ba2 + 0x5c)))   # symbol + offset from the log
```
(For caller frames subtract 1 from the address: they're return addresses.)

**Root cause:** peer 4 isn't in `mPeers` yet, so `peer` is `nullptr` and `peer->matchIndex` faults at address `0x8`
(the offset of `matchIndex` in `Peer`). **A fault address close to 0 almost always means a member access through a null pointer**.
The number is the member's offset.

**Why re-raise?** Handling SIGSEGV and returning would re-execute the faulting instruction forever;
calling `_exit` would lose the core. Resetting to `SIG_DFL` and re-raising gives you both the log and the core.
**In Gringofts:** add `absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions())` (target
`absl::failure_signal_handler`) at the top of `main()` in `Main.cpp`. Abseil is already in `third_party/`.

---

## Bonus — static analysis results (clang-tidy 18, this repo's `.clang-tidy`)
Measured, not guessed. `scripts/lint.sh crash_lab` reports:

| Lab | Caught? | Finding |
|---|---|---|
| 14 uninitialised member | **yes, twice** | `cppcoreguidelines-pro-type-member-init`: constructor does not initialize `mIsLeader`; `clang-analyzer-core.uninitialized.UndefReturn`: garbage value returned from `isLeader()` |
| 13, 15 | incidental | `pro-type-member-init` on aggregate fields (`Session::lastSeenMs`, `Account::balance`). Worth fixing, but not the lab's bug |
| 01, 02, 03, 04, 05, 06, 07, 08, 10, 11, 12, 13, 15, 16 | **no** | null derefs, dangling references, heap overflow, recursion, iterator invalidation, lifetime-in-destructor, shared_ptr cycles, lock order and races all pass clean. `clang -Wall -Wextra` is silent on all 16 too |
| 09 reentrant lock | **only with annotations** | see below |

(clang-tidy's `concurrency-mt-unsafe` also flagged `std::localtime` in the labs' own logging macro, a real
thread-safety bug in the helper. It's fixed now: `localtime_r`.)

**Takeaway:** turn on clang-tidy and treat its findings as bugs, but most of the costly C++ failures are
only found **at run time**: ASan/UBSan/TSan in CI, plus good crash diagnostics in production.

### Clang thread-safety analysis catches lab 09 at compile time
Annotate which mutex guards which data and which functions take the lock. Abseil (already in
Gringofts' `third_party/`) provides `absl::Mutex` with the annotations built in, plus the macros
`ABSL_GUARDED_BY`, `ABSL_LOCKS_EXCLUDED`, `ABSL_EXCLUSIVE_LOCKS_REQUIRED`.
```cpp
class RaftRole {
 public:
  uint64_t term() const ABSL_LOCKS_EXCLUDED(mMutex) { absl::MutexLock l(&mMutex); return mTerm; }
  void becomeLeader() ABSL_LOCKS_EXCLUDED(mMutex) { absl::MutexLock l(&mMutex); ++mTerm; term(); }
 private:
  mutable absl::Mutex mMutex;
  uint64_t mTerm ABSL_GUARDED_BY(mMutex) = 1;
};
```
`clang++ -Wthread-safety` then reports (verified with an equivalent hand-annotated wrapper):
```
warning: cannot call function 'term' while mutex 'mMutex' is held [-Wthread-safety-analysis]
warning: reading variable 'mTerm' requires holding mutex 'mMutex'      (for any unguarded access)
```
It works with Clang only (GCC ignores the attributes), and only on code you annotate. That makes it a good fit for the few
classes in Gringofts where several threads share state.
