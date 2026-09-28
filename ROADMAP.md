# C++ for an advanced Java developer → contributing to Gringofts

A 4-week, ~1.5–2 h/day plan. It is ordered by **what you need to read and
change Gringofts code**, not by textbook order. Every phase ends with an
exercise in this repo (`exercises/NN_*`) and a reading task in Gringofts
(`GRINGOFTS_GUIDE.md`).

Gringofts facts that shape this plan:
- **C++17**, built with **CMake**, CI uses **g++-9** → no C++20 (`concepts`,
  `ranges`, `std::format`, `std::jthread`, `<span>`, designated initialisers are all out).
- Style: Google C++ style + **cpplint** (`make gringofts_check`), 120-char lines,
  `mMember` field naming, `kConstant` constants.
- Libraries: **gRPC + protobuf** (RPC & serialization), **RocksDB** (Raft log /
  state storage), **SQLite**, **spdlog** (`SPDLOG_INFO(...)`), **INIReader**
  (config), **prometheus-cpp** (metrics), **abseil**, **Boost**, **OpenSSL**,
  **gtest/gmock** (tests).
- Architecture: event sourcing + Raft. A handful of long-running threads
  ("loops") connected by blocking queues, all heavy on `std::shared_ptr`,
  `std::unique_ptr`, `std::atomic`, `std::mutex`.

---

## Week 1 — The language model that differs from Java

| Day | Topic | Exercise | Key idea to internalise |
|---|---|---|---|
| 1 | Toolchain: compile, link, headers, CMake | `00_build_basics` | a `.cpp` is a translation unit; "undefined reference" is a *linker* error |
| 2 | Values, references, pointers, `const` | `01_references_pointers` | `Foo f = g;` **copies the object** |
| 3 | Object lifetime, destructors, RAII | `02_raii` | cleanup is deterministic; no `finally` needed |
| 4 | Copy vs move, Rule of 0/3/5 | `03_rule_of_five` | `std::move` is a cast; moved-from objects are valid-but-unspecified |
| 5 | Smart pointers & ownership | `04_smart_pointers` | `unique_ptr` by default; raw `T*` = non-owning |

**Checkpoint:** you can explain, for any parameter or member in
`src/app_demo/should_be_generated/app/App.h`, *who owns it and when it dies*.

## Week 2 — Abstractions: OOP, templates, the standard library

| Day | Topic | Exercise | Key idea |
|---|---|---|---|
| 6 | Virtual functions, interfaces, factories | `05_polymorphism` | non-virtual by default; virtual destructor in every base |
| 7 | Templates, `if constexpr`, variadics | `06_templates` | code generated per type → lives in headers (`.hpp`) |
| 8 | STL containers, algorithms, lambdas | `07_stl_lambdas` | `map[k]` inserts; iterator invalidation; lambda captures can dangle |
| 9 | `optional`, `string_view`, exceptions vs status codes | `08_optional_errors` | three error styles coexist in Gringofts |
| 10 | Buffer / catch-up day | reread all "Think about" sections | |

**Checkpoint:** read `src/infra/es/Command.h`, `Event.h`, `StateMachine.h`,
`src/infra/mpscqueue/MpscQueue.h` and explain every keyword on every line
(`virtual`, `= 0`, `override`, `const`, `= default`, `static constexpr`, `std::optional`, `std::string_view`).

## Week 3 — Concurrency, testing, and the build

| Day | Topic | Exercise | Key idea |
|---|---|---|---|
| 11 | `std::thread`, `mutex`, `condition_variable` | `09_threads_queue` | a joinable `std::thread` destroyed = `std::terminate()` |
| 12 | `std::atomic`, loop objects, clean shutdown | `10_loop_atomics` | data races are UB; use TSan |
| 13 | gtest / gmock | `11_gmock` | mocks need virtual methods → design to interfaces |
| 14 | Sanitizers & debugging | re-run 04, 09, 10 with `-DJ2C_SANITIZER=address/thread`; `gdb`/`lldb` basics (below) | most C++ bugs are found by tools, not by staring |
| 15 | Build Gringofts (Docker) and run its tests | `GRINGOFTS_GUIDE.md` G1 | |

## Week 4 — Gringofts itself

| Day | Topic | Where |
|---|---|---|
| 16 | Capstone: mini event-sourced app + new command | `12_event_sourcing_capstone` |
| 17 | Protobuf & gRPC (sync/async servers, `CompletionQueue`) | `GRINGOFTS_GUIDE.md` G2–G3 |
| 18 | Trace a request end to end through the demo app | G3 |
| 19–20 | First real change: add `DecreaseCommand` to `app_demo` with tests | G4 |
| 21+ | Raft internals (`src/infra/raft/v2/RaftCore.cpp`), storage, metrics | G5–G7 |

---

## Debugging cheat sheet
```bash
# build with symbols (Debug is the default in this repo)
gdb --args ./build/ex09 --gtest_filter='MpscQueue.*'
(gdb) run            # reproduce
(gdb) bt             # stack trace of the crashing thread
(gdb) thread apply all bt   # all threads (deadlocks!)
(gdb) frame 3 ; info locals ; p *this
```
- **Segfault** → run under ASan first (`-DJ2C_SANITIZER=address`); it prints
  the exact use-after-free / out-of-bounds with allocation + free stacks.
- **Hang** → `gdb -p <pid>` then `thread apply all bt`; look for two threads
  each waiting on a mutex the other holds.
- **Flaky test** → `--gtest_repeat=1000 --gtest_break_on_failure` under TSan.
- **Undefined behaviour** → `-DJ2C_SANITIZER=undefined`.

## Books & references (in priority order)
1. *A Tour of C++* (3rd ed.), Stroustrup — 250 pages, the fastest overview for an experienced programmer.
2. *Effective Modern C++*, Scott Meyers — items on move semantics, smart pointers, lambdas, concurrency. Exactly the C++11–17 subset Gringofts uses.
3. [cppreference.com](https://en.cppreference.com) — your Javadoc. Check the "since C++XX" tags against C++17.
4. [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) — sections R (resources), C (classes), CP (concurrency).
5. [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) — what cpplint in Gringofts enforces.
6. *C++ Concurrency in Action* (2nd ed.), Anthony Williams — when you get to Raft internals.
7. [gRPC C++ async tutorial](https://grpc.io/docs/languages/cpp/async/) — before touching `RequestReceiver`.
