# java2cpp

Rapid onboarding from Java to C++, aimed at contributing to
[eBay/Gringofts](https://github.com/eBay/Gringofts) (C++17 event-sourcing + Raft framework).

| Doc | What it is |
|---|---|
| [ROADMAP.md](ROADMAP.md) | 5-week day-by-day plan (Raft treated as a black box), reading list |
| [DEBUGGING.md](DEBUGGING.md) | production debugging playbook: exit codes & abort messages, core dumps (local/systemd/Docker/k8s), gdb, sanitizers, hangs, leaks, build/link errors |
| [JAVA_TO_CPP_CHEATSHEET.md](JAVA_TO_CPP_CHEATSHEET.md) | syntax map, parameter-passing rules, "compiles but is a bug" list, reading compiler errors |
| [GRINGOFTS_GUIDE.md](GRINGOFTS_GUIDE.md) | codebase map, request trace, and real contribution tasks (G1–G7) |
| [`exercises/`](exercises) | 13 test-driven exercises, each tied to Gringofts code |
| [`crash_lab/`](crash_lab/README.md) | 16 deliberately broken programs: segfaults, aborts, deadlocks, races, leaks, heap corruption, stripped prod binaries. Diagnose from the core/log, then fix |

## Exercises

Each exercise has a `README.md` (concept + tasks), a `test.cpp` (gtest, already
written), and `src/` (starter code with `TODO`s). You make the tests pass.

| # | Topic | Gringofts pattern it prepares you for |
|---|---|---|
| 00 | headers, sources, linking, namespaces | every file; `StrUtil.h` |
| 01 | values vs references vs pointers, `const` | `process(const State &, const Cmd &, std::vector<...> *events)` |
| 02 | RAII, destructors, `ScopeGuard`, move-only handles | `std::lock_guard`, RocksDB/file handles |
| 03 | copy/move, Rule of 0/3/5, `noexcept` | `std::move` of commands through queues |
| 04 | `unique_ptr` / `shared_ptr` / `weak_ptr` | `App.h` members, `shared_ptr<Command>`/`<Event>` |
| 05 | virtual, interfaces, factories, `dynamic_cast` | `Encodable`, `Event`, `EventDecoderImpl` |
| 06 | templates, `if constexpr`, variadics | `MpscQueue<T>`, `CommandProcessLoop<StateMachineType>` |
| 07 | STL containers, algorithms, lambdas | peer maps in Raft, callbacks |
| 08 | `optional`, `string_view`, exceptions, INI config | `INIReader`, `ProcessHint`, `dedupId()` |
| 09 | threads, mutex, condvar: blocking MPSC queue | `MpscDoubleBufferQueue`, `BlockingQueue` |
| 10 | atomics, thread-owning loops, prompt shutdown | `CommandProcessLoop`, `EventApplyLoop`, `App::shutdown()` |
| 11 | gtest fixtures, gmock `MOCK_METHOD` / `EXPECT_CALL` | `test/`, `ReadonlyCommandEventStoreMock.h` |
| 12 | **capstone**: mini event-sourced app, add a new command | `app_demo` end to end |

## Quick start

Needs a C++17 compiler (g++ ≥ 9 or clang ≥ 10) and CMake ≥ 3.16 (3.20+ for `ctest --test-dir`; on older CMake, `cd build && ctest`). GoogleTest is
downloaded automatically if it isn't installed.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure      # everything fails at first: that's the point
```

Work on one exercise at a time:
```bash
cmake --build build --target ex03 && ./build/ex03                  # run one exercise
./build/ex03 --gtest_filter='BufferTest.Move*'                      # run a subset
ctest --test-dir build -R 03_ --output-on-failure                  # via ctest
```

Stuck? Reference answers are in `solutions/`. To check that they pass:
```bash
cmake -S . -B build-solutions -DJ2C_USE_SOLUTIONS=ON && cmake --build build-solutions -j
ctest --test-dir build-solutions
```

Run under sanitizers (do this for 02, 03, 04, 09, 10 — it's how you'll debug Gringofts):
```bash
cmake -S . -B build-asan -DJ2C_SANITIZER=address   && cmake --build build-asan -j && ctest --test-dir build-asan
cmake -S . -B build-tsan -DJ2C_SANITIZER=thread    && cmake --build build-tsan -j && ctest --test-dir build-tsan
cmake -S . -B build-ubsan -DJ2C_SANITIZER=undefined
```

Every test has a 20 s timeout under ctest, so a deadlock in your queue shows
up as a failure instead of a hang.

## Crash lab (debugging practice)

```bash
crash_lab/scripts/run_lab.sh 01          # runs lab01 with core dumps on, prints the gdb command
gdb build/crash_lab/lab01 cores/core.lab01
BUILD=build-asan crash_lab/scripts/run_lab.sh 07   # same lab, AddressSanitizer build
build/crash_lab/lab16 2>&1 | crash_lab/scripts/symbolize.sh   # logs-only crash -> file:line
```
Missions are in [`crash_lab/README.md`](crash_lab/README.md), answers in [`crash_lab/ANSWERS.md`](crash_lab/ANSWERS.md).
Needs `gdb`; `valgrind` is used by lab 14. The labs are plain executables, so `ctest` doesn't run them.

## Suggested workflow per exercise (~1–2 h)
1. Read the exercise `README.md`, then the linked Gringofts files.
2. Read `test.cpp` — the tests are the spec.
3. Fill in the `TODO`s in `src/` until `./build/exNN` is green.
4. Do the "Experiments" / "Think about" section — break things on purpose and read the errors.
5. Diff against `solutions/NN_*/` and note anything idiomatic you missed.
