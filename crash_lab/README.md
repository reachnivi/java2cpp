# Crash lab: practise debugging real C++ failures

Sixteen small programs, each broken in a way you **will** meet maintaining a
long-running, multi-threaded C++ service like Gringofts. Each one looks like
Gringofts code (loops, queues, commands, events, peers). You start where you'd
start in production: a symptom, a log, maybe a core file. You finish with the
root cause and a fix.

The playbook these labs teach is in [`../DEBUGGING.md`](../DEBUGGING.md).
Answers are in [`ANSWERS.md`](ANSWERS.md). Don't open it until you've written
down your own diagnosis.

## Setup
```bash
cmake -S . -B build && cmake --build build -j        # builds exercises + labs (labNN)
crash_lab/scripts/run_lab.sh 01                       # runs lab01 with cores on, prints the gdb command
gdb build/crash_lab/lab01 cores/core.lab01
```
Sanitizer builds of the labs (you'll need them for several missions):
```bash
cmake -S . -B build-asan -DJ2C_SANITIZER=address && cmake --build build-asan -j
cmake -S . -B build-tsan -DJ2C_SANITIZER=thread  && cmake --build build-tsan -j
BUILD=build-asan crash_lab/scripts/run_lab.sh 07
```

## How to work each lab
1. **Run it and write down the symptom exactly**: exit status, signal, the
   last log line, any message from glibc or libstdc++. That text is your best clue.
2. **Pick a tool from the symptom** (see the table in `DEBUGGING.md`), *before*
   reading the source.
3. **Find the crash site**, then **find the bug site**. They are often
   different functions, sometimes different threads.
4. Fix it, rerun, and rerun under the relevant sanitizer.
5. Compare with `ANSWERS.md` and note which step you'd have skipped.

Try to get the answer from the tools first. Read the whole source only when
the tools have pointed you at a line.

---

## The labs

| Lab | Symptom | Main tools |
|---|---|---|
| 01 | SIGSEGV during startup recovery | core + `bt`, `frame`, `print` |
| 02 | SIGABRT on every shutdown: `terminate called after throwing ...` | core + `bt` in the right thread, `catch throw` |
| 03 | SIGABRT: `terminate called without an active exception`, error never logged | core + `bt`, reasoning about constructors |
| 04 | `pure virtual method called` on shutdown | core + `bt`, object lifetime |
| 05 | `double free or corruption` / `free(): invalid pointer` | core, ASan |
| 06 | wrong output or a crash, depending on build | ASan |
| 07 | `malloc(): corrupted top size`; backtrace blames innocent code | ASan (a core alone misleads you) |
| 08 | hang, 0% CPU | `gcore` / `gdb -p`, `thread apply all bt`, mutex owner |
| 09 | hang, single thread | `gdb -p`, `bt` |
| 10 | occasional crash or wrong counts | TSan |
| 11 | SIGSEGV with a backtrace 150,000 frames deep | `bt N` / `bt -N` |
| 12 | memory grows until OOM-kill (exit 137), no core | LeakSanitizer, counting objects |
| 13 | crash or silently skipped entries | `-D_GLIBCXX_DEBUG`, ASan |
| 14 | wrong behaviour in some builds only | valgrind `--track-origins=yes`, `-Wuninitialized` |
| 15 | crash in an optimised, stripped production binary | separate debug info, `<optimized out>` |
| 16 | only a log is available, no core | crash handler output + `symbolize.sh` / `addr2line` |

### 01 — null deref during recovery
Run `run_lab.sh 01`. In gdb: `bt`, then `frame 1`, `frame 2`, and `info locals` / `print` in each.
- Which frame did the crash happen in, and which frame has the bug?
- What does `print entry` show in the `replayOne` frame? Does the log line before the crash agree?
- What should recovery do with an unknown type? (Think about rolling upgrades.)

### 02 — crash on shutdown
- Which thread crashed? (`info threads`, `thread N`, `bt`.) Is it the main thread?
- Find the frame that threw. Why did nobody catch the exception?
- Bonus: run it live under gdb with `catch throw` and see where it stops.
- Fix it so shutdown is clean and the loop still exits promptly.

### 03 — error never logged
Run it with `run_lab.sh 03 abc` and with `run_lab.sh 03 8080`.
- `main` catches `std::exception`. So why does the process abort, and why is the log line missing?
- Which destructor ran, and which one did **not**? (C++ rule: if a constructor throws, ...)
- Give two fixes: one that changes the order in the constructor, one that makes the class safe whatever the order.

### 04 — pure virtual method called
- In the backtrace, which thread calls the pure virtual function, and from where?
- At that moment, which destructors have already finished? What is the object's dynamic type at that moment?
- Where should `stop()` be called so this can't happen? What does that mean for designing base classes that own threads?

### 05 — double free
- Read the glibc message. Then run it under ASan (`BUILD=build-asan`). What does ASan call it?
- How many control blocks own the `Command`?
- What's the idiomatic way for an object to hand out a `shared_ptr` to itself?

### 06 — works on my machine
- Run the normal build, then the ASan build. Read ASan's three stacks: the bad **access**, the **free**, and the original **allocation**.
- Which line freed the memory the reference points into? Why?
- List three other standard-library operations that invalidate references in the same way.

### 07 — heap corruption
- Get a core and `bt`. Which function does the backtrace blame? Is it wrong?
- Now run the ASan build. Where is the actual out-of-bounds write?
- Why does heap corruption usually crash *later*, somewhere else?

### 08 — deadlock
Run `run_lab.sh 08` (it snapshots the hung process with `gcore`), then `gdb build/crash_lab/lab08 cores/core.lab08`.
- `thread apply all bt`: which two threads are blocked, and on which source lines?
- In each blocked frame, `print mLogMutex` / `print mStateMutex`: the `__owner` field is the LWP (thread id) holding it. Draw the wait-for graph.
- Fix it two ways: consistent lock order, and `std::scoped_lock(a, b)`.

### 09 — single-threaded hang
- Attach with `gdb -p $(pgrep lab09)` while it hangs, or use `run_lab.sh 09`. What is the thread waiting on?
- Why would this code work with Java's `synchronized`?
- Fix it without switching to `std::recursive_mutex`. (Hint: private `...Locked()` helpers that assume the lock is already held.)

### 10 — data race
- Run the normal build 5 times. Are the results consistent?
- Run the TSan build. Read the two stacks it prints: the two conflicting accesses and the threads that made them.
- Fix it. Is `std::atomic` enough here? Why not?

### 11 — stack overflow
- `bt` never ends. Use `bt 5` (innermost) and `bt -5` (outermost). What repeats?
- Why does `apply(event)` not call the `ProcessedEvent` overload? (Static vs dynamic type in overload resolution.)
- Fix the dispatch. Is this how Gringofts' `AppStateMachine` does it? (`registerEventApplier` + `dynamic_cast`)

### 12 — leak without `new`
- `inflight` is empty, yet `alive Commands` keeps growing. Who still owns them?
- Run the ASan build (LeakSanitizer is on by default). Which allocation does it report?
- Two fixes: break the cycle with `std::weak_ptr`, or don't store the callback in the object it captures. Then discuss: why does Java's GC handle this cycle and `shared_ptr` doesn't?

### 13 — erase while iterating
- Run the normal build, then build it with `-D_GLIBCXX_DEBUG` (see ANSWERS for the command). Read the message.
- Fix both loops (map and vector) with the `it = container.erase(it)` pattern, then rewrite the vector one with erase–remove.

### 14 — uninitialised member
- Run it. Then `valgrind --track-origins=yes build/crash_lab/lab14`. What does valgrind point at, and where did the uninitialised value come from?
- Why does the Debug build "never" do it? Why did adding a log line "fix" it?
- Which compiler warning would have caught it? Which cpplint / clang-tidy check?

### 15 — production crash
`run_lab.sh 15` runs `lab15_stripped` like production would.
- `gdb build/crash_lab/lab15_stripped cores/core.lab15` → `bt`. What can you see?
- Now load the debug info: inside gdb, `symbol-file build/crash_lab/lab15.debug`, then `bt`, `info locals`. What is `<optimized out>`, and how can you still work out `src` and `dst`?
- Which build and packaging settings would you ask for so every production crash is debuggable? (See `DEBUGGING.md`, "Make production crashes debuggable".)

### 16 — logs only
Run `build/crash_lab/lab16 2>&1 | crash_lab/scripts/symbolize.sh`.
- Read `crash_handler.h`. Why must the handler re-raise the signal?
- Which frame is the handler itself, which is the kernel's signal trampoline, and which is the real crash?
- Resolve the crashing frame by hand with `nm` + `addr2line` (the script shows how), so you can do it on a machine without this script.
- Gringofts vendors Abseil: find `absl::InstallFailureSignalHandler` and sketch how you'd enable it in `Main.cpp`.
