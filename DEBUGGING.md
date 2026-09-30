# Debugging C++ in production: a playbook for Java developers

In Java, a bug usually ends in an exception with a stack trace in the log. In C++ it
can end in a signal and a core file, a hang, slowly growing memory, or wrong answers
that only show up in Release builds. This playbook goes from **symptom to tool to
root cause**. Practise each part in the [crash lab](crash_lab/README.md).

---

## 1. Triage: read the symptom first

### Exit status
| Exit status | Meaning | Typical cause | First move |
|---|---|---|---|
| 139 (128+11) | SIGSEGV | null / dangling pointer, stack overflow, heap corruption | core → `bt` (labs 01, 11, 15, 16) |
| 134 (128+6) | SIGABRT | `std::terminate`, `assert`, glibc heap check, `GPR_ASSERT` in gRPC | read the **message printed just before**; core → `bt` (02–05, 07) |
| 137 (128+9) | SIGKILL | **OOM killer** (or `kill -9`, or k8s liveness kill) | `dmesg \| grep -i oom`, `kubectl describe pod` → `OOMKilled`; no core is written (12) |
| 136 (128+8) | SIGFPE | integer divide by zero | core → `bt` |
| 135 (128+7) | SIGBUS | mmap'd file truncated underneath you, misaligned access | core → `bt`; check file sizes (RocksDB/segment files) |
| 141 (128+13) | SIGPIPE | writing to a closed socket/pipe | ignore SIGPIPE or handle `EPIPE` (gRPC handles its own sockets) |
| no exit, 0% CPU | deadlock / waiting forever | lock-order inversion, self-deadlock, lost notify | `gcore` / `gdb -p` → `thread apply all bt` (08, 09) |
| no exit, 100% CPU | livelock / infinite loop / spin | | `perf top -p PID`, then `gdb -p` a few times and compare `bt`s |
| exits 0, wrong answer | undefined behaviour or logic | uninitialised, dangling, races | sanitizers, valgrind (06, 10, 13, 14) |

### Messages that tell you what happened
| Message on stderr | What it means | Lab |
|---|---|---|
| `terminate called after throwing an instance of 'X'` | an exception escaped a thread function, a `noexcept` function, or a destructor | 02 |
| `terminate called without an active exception` | usually a **joinable `std::thread` destroyed** | 03 |
| `pure virtual method called` | virtual call on an object being constructed/destroyed (thread outlived the object) | 04 |
| `double free or corruption`, `free(): invalid pointer` | two owners, or deleting something not from `new` | 05 |
| `malloc(): corrupted top size`, `free(): invalid next size`, `corrupted size vs. prev_size` | **heap buffer overflow happened earlier**; the backtrace is misleading → ASan | 07 |
| `Assertion 'x' failed.` | `assert()` fired (only when `NDEBUG` is not defined) | |
| `*** stack smashing detected ***` | stack buffer overflow (local array) → ASan | |
| `Segmentation fault ... at address 0x8` | small address = **null pointer + member offset** | 16 |
| `error while loading shared libraries: libX.so` | runtime linker can't find a `.so` → `ldd ./binary`, `LD_LIBRARY_PATH` | |
| `undefined symbol: _ZN...` (at start-up) | binary/library version mismatch → `c++filt _ZN...`, `nm -D lib.so \| grep` | |

**Crash site ≠ bug site.** The line that faulted is where bad data was *used*. The bug is
where the bad data was *created*: a null returned two frames up, a reference taken before
a `push_back`, a buffer overrun in a function that returned long ago.

---

## 2. Getting a core file

### Local machine
```bash
ulimit -c unlimited                    # per shell; 0 by default on most distros
cat /proc/sys/kernel/core_pattern      # where cores go
```
- `core` or `core.%p` → written to the process's **current directory** (must be writable, with free space).
- `|/usr/lib/systemd/systemd-coredump ...` → `coredumpctl list`, `coredumpctl info <pid>`, `coredumpctl gdb <pid>`, `coredumpctl dump <pid> -o core`.
- `|/usr/share/apport/apport ...` (Ubuntu) → `/var/crash/*.crash`; unpack with `apport-unpack`. Apport ignores
  binaries not installed from packages, so for dev work you may need `sudo systemctl stop apport` or a plain
  `core_pattern` (`sudo sysctl -w kernel.core_pattern=/tmp/core.%e.%p`).
- setuid binaries and processes that changed uid don't dump unless `fs.suid_dumpable` allows it.

### systemd service
```ini
[Service]
LimitCORE=infinity
```

### Docker / Kubernetes
`core_pattern` is a **host kernel** setting shared by all containers. It is interpreted inside the
container's filesystem if it's a path, or run on the **host** if it's a pipe.
```bash
docker run --ulimit core=-1 -v /srv/cores:/cores ...   # with host core_pattern=/cores/core.%e.%p
# attaching gdb inside a container:
docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined ...
```
On Kubernetes, ask platform ops how cores are collected (a node-level `core_pattern` + hostPath, or a core-dump handler DaemonSet).

### No crash, but you want a core
```bash
gcore -o /tmp/snap <pid>     # snapshot a live (e.g. hung) process; it keeps running
kill -ABRT <pid>             # kill it AND dump core (SIGQUIT works too)
```

---

## 3. gdb essentials

```bash
gdb ./binary core                  # post-mortem
gdb -p <pid>                       # attach to a live process (pauses it!)
gdb --args ./binary --flag x       # run under gdb
gdb -batch -ex 'thread apply all bt full' ./binary core > bt.txt   # share with the team
```
Put this in `~/.gdbinit`: `set pagination off`, `set print pretty on`, `set print object on`
(show the dynamic type through base pointers).

| Command | Use |
|---|---|
| `bt` / `bt full` | backtrace (with locals) |
| `bt 10` / `bt -10` | innermost / outermost 10 frames (stack overflows) |
| `frame N` / `up` / `down` | move between frames |
| `info locals`, `info args`, `print x`, `print *this`, `print *ptr@10` | inspect |
| `ptype x`, `whatis x` | type of an expression |
| `print vec`, `print map`, `print sp` | STL pretty-printers (sizes, elements, `shared_ptr` use counts) |
| `info threads`, `thread N`, `thread apply all bt` | multi-threaded programs: **always** look at all threads |
| `print mMutex` → `__owner = LWP` | which thread holds a `std::mutex` (lab 08) |
| `info registers`, `x/8gx $sp`, `disassemble` | when variables are `<optimized out>` |
| `info sharedlibrary` | which `.so`s were loaded, and whether they have symbols |
| `catch throw` | stop where an exception is thrown (live only) |
| `break file.cpp:42 if id == 7` | conditional breakpoint |
| `watch -l obj->mField` | stop when memory changes (who corrupts this?) |
| `handle SIGPIPE nostop noprint` | ignore noisy signals |
| `symbol-file x.debug` / `set debug-file-directory` | load separate debug info (lab 15) |

Reading a backtrace:
- Frames inside `libc`/`libstdc++`/`libgrpc` are rarely the bug. Walk `up` to the first frame in your code.
- `??` means no symbols for that frame: wrong binary, stripped binary, or missing debug info.
- `<optimized out>` means compiled with `-O2`. Look at the caller's arguments or registers, or reproduce with `-O0`.
- Fault address `0x0`–`0xfff`: null pointer plus a member offset. Repeating patterns like `0xabababab` or `0xdeadbeef`:
  uninitialised or freed memory. Large random addresses: use-after-free or a corrupted pointer.

---

## 4. The dynamic-analysis toolbox

| Tool | Finds | How | Cost |
|---|---|---|---|
| **AddressSanitizer** (ASan) | heap/stack overflow, use-after-free, double free, leaks (LSan) | `-fsanitize=address -fno-omit-frame-pointer -g` | ~2× slower, ~2–3× memory |
| **UndefinedBehaviorSanitizer** | signed overflow, bad casts, misaligned loads, null member calls | `-fsanitize=undefined` | small; combine with ASan |
| **ThreadSanitizer** (TSan) | data races, lock-order inversions (potential deadlocks) | `-fsanitize=thread` (cannot combine with ASan) | 5–15× slower |
| **libstdc++ debug mode** | invalid iterators, out-of-range `[]`, bad comparators | `-D_GLIBCXX_DEBUG` (checked containers, changes ABI) or `-D_GLIBCXX_ASSERTIONS` (cheap checks, ABI-safe) | moderate |
| **valgrind memcheck** | uninitialised reads, leaks, invalid access, **no rebuild needed** | `valgrind --track-origins=yes --leak-check=full ./bin` | 20–50× slower |
| valgrind helgrind / drd | races and lock misuse without rebuilding | `valgrind --tool=helgrind` | very slow |
| heaptrack / valgrind massif | *who* allocates the growing memory | `heaptrack ./bin`, `valgrind --tool=massif` | |
| perf | CPU hot spots, spins | `perf top -p PID`, `perf record -g` | low |
| strace | syscalls: file opens, blocking reads, `EPIPE`, `ENOSPC` | `strace -f -tt -p PID` | high per syscall |

Notes:
- Sanitizers see only code **compiled** with them. Accesses inside prebuilt libraries (libstdc++, gRPC,
  RocksDB) can be missed (ASan) or cause false reports (TSan). Lab 06's ANSWERS shows a real example.
- Run the **unit tests** under ASan+UBSan and TSan in CI. That's where most of these bugs get caught cheaply.
- Uninitialised memory isn't found by ASan. Use valgrind, or MemorySanitizer (clang only, needs everything rebuilt).

---

## 5. Recipes by symptom

### Hang / deadlock (labs 08, 09)
1. `top -H -p PID`: 0% CPU means blocked; 100% on one thread means spinning.
2. `gcore PID` (non-destructive) or `gdb -p PID` → `thread apply all bt`.
3. Find threads in `futex_wait` / `__lll_lock_wait` / `pthread_cond_wait`. For each held mutex, `print` it and read `__owner`.
4. Draw who-holds-what and who-waits-for-what. A cycle means lock-order inversion. A thread waiting on a lock it holds itself means re-entrancy.
5. Condition-variable hangs: check that every `wait` has a predicate and that the notifier changes the state **under the mutex**.

### Memory grows (lab 12)
1. Confirm it's growth, not a cache warming up: RSS over hours (`ps -o rss`, Prometheus `process_resident_memory_bytes`).
2. Count live objects of your main types (a static counter, as lab 12 does) and compare with what *should* be alive.
3. ASan/LSan at exit for true leaks. heaptrack/massif for "reachable but unbounded" growth (caches, queues, maps that never shrink).
4. Usual causes in this style of code: `shared_ptr` cycles through stored callbacks, maps keyed by request id that are never erased,
   unbounded queues when the consumer is slower than producers.

### Crash only in Release / only in prod / "Heisenbug" (labs 06, 10, 14)
Almost always undefined behaviour: uninitialised members, dangling references, data races,
signed overflow, a missing `return`. Run the ASan+UBSan build, then TSan, then valgrind. Treat
every compiler warning (`-Wall -Wextra`) as a bug report. Gringofts builds Debug with `-Werror` for this reason.

### Logs vanish right before the crash
- `stdout` is **fully buffered** when redirected to a file or pipe (line-buffered only on a terminal), so the last lines can be lost on a crash.
  `stderr` is unbuffered.
- spdlog: file sinks and async loggers buffer too. Use `spdlog::flush_on(spdlog::level::err)` and
  `spdlog::flush_every(std::chrono::seconds(1))`, and call `spdlog::shutdown()` on clean exit.
  Gringofts' `Main.cpp` only sets the log pattern, so check the sinks your deployment configures.
- A **crash handler** that prints a backtrace with `write(2)` from the signal handler survives all of this (lab 16).

---

## 6. Make production crashes debuggable (discuss with your team)

A core is useful only if you have **the exact binary** and **its debug info**. Checklist:
1. **Build with symbols even when optimised:** `-O2 -g` (CMake's `RelWithDebInfo`), plus `-fno-omit-frame-pointer`
   for reliable stacks and cheap profiling.
2. **Ship stripped, archive the debug info** per release, keyed by build-id (lab 15):
   ```bash
   objcopy --only-keep-debug app app.debug
   strip --strip-debug --strip-unneeded app
   objcopy --add-gnu-debuglink=app.debug app
   readelf -n app | grep 'Build ID'          # match core ↔ binary ↔ debug file
   ```
3. **Enable cores** in the deployment (ulimit / `LimitCORE` / container settings) with a destination that has space.
4. **Install a failure signal handler** so logs contain a stack trace even without a core:
   Abseil is already vendored in Gringofts: `absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());`
   early in `main()`, linking `absl::failure_signal_handler`.
5. **Keep assertions meaningful.** Decide deliberately whether `assert` is active in production. In Gringofts' `CMakeLists.txt`
   a comment says asserts should stay on in Release, but the line only *appends* `-O2 -Werror` to CMake's default
   `CMAKE_CXX_FLAGS_RELEASE` (normally `-O3 -DNDEBUG`), so `NDEBUG` may still be defined. Also, `-g -O0 --coverage` are put in the base
   `CMAKE_CXX_FLAGS` for every build type. **Check what production actually compiles with** (`make VERBOSE=1`
   or `compile_commands.json`) before assuming either way. It decides whether a core will have symbols, whether
   `assert`s fire, and what performance you get.
6. libstdc++ and libgcc are linked **statically** (`-static-libgcc -static-libstdc++`), so their code lives inside your
   binary (frames get function names from it, usually without line numbers), and `GLIBCXX_x.y.z not found`
   errors can't happen at deploy time.

---

## 7. Build and link errors (the other common daily issue)

| Error | Cause | Fix |
|---|---|---|
| `undefined reference to 'Foo::bar()'` | declared, not defined or not linked | add the `.cpp` to the CMake target, or the library to `target_link_libraries`; check for a missing `inline`/template definition in the header |
| `undefined reference to 'vtable for Foo'` | a virtual function (often the destructor) declared but never defined | define it (`= default` counts), or make it pure |
| `multiple definition of 'x'` | non-`inline` function/variable defined in a header | `inline`, or move it to the `.cpp` |
| `incomplete type 'Foo' used` | only a forward declaration is visible | include the real header in the `.cpp` |
| `use of deleted function` | copying a move-only type (`unique_ptr`, `thread`, `mutex`) | `std::move`, a reference, or `emplace_back` |
| wall of template errors | a type doesn't meet a template's needs | read the **first** error and the `required from here` line |
| `'foo.pb.h' file not found` | protobuf code not generated yet / include path | build the proto target first; check `include_directories` for `generated/` |
| link errors with `std::__cxx11::basic_string` | mixing libraries built with different `_GLIBCXX_USE_CXX11_ABI` | rebuild the dependency with the same compiler and flags |

Tools: `nm -C lib.a | grep Foo` (is it defined? `T` = defined, `U` = undefined), `c++filt` (demangle),
`ldd ./bin` (runtime `.so` resolution), `readelf -d ./bin` (NEEDED/RPATH), `make VERBOSE=1` (the exact command line).
