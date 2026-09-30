# C++ for an advanced Java developer → contributing to Gringofts

A 5-week, ~1.5–2 h/day plan. It is ordered by **what you need to read and
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

## Week 3 — Concurrency, testing, and your first core dumps

| Day | Topic | Where | Key idea |
|---|---|---|---|
| 11 | `std::thread`, `mutex`, `condition_variable` | `09_threads_queue` | a joinable `std::thread` destroyed = `std::terminate()` |
| 12 | `std::atomic`, loop objects, clean shutdown | `10_loop_atomics` | data races are UB; use TSan |
| 13 | gtest / gmock | `11_gmock` | mocks need virtual methods → design to interfaces |
| 14 | Core dumps + gdb; the `std::terminate` family | crash lab 01–04, `DEBUGGING.md` §1–3 | read the stderr message, then `bt` on the **right thread** |
| 15 | Memory errors | crash lab 05–07 + ASan | crash site ≠ bug site; glibc heap aborts mean "go to ASan" |

## Week 4 — Hangs, leaks, production crashes

| Day | Topic | Where | Key idea |
|---|---|---|---|
| 16 | Deadlocks and races | crash lab 08–10, `gcore`, TSan | `thread apply all bt` + mutex `__owner` |
| 17 | Stack overflow, leaks, iterators, uninitialised state | crash lab 11–14 | exit 137 = OOM kill, no core; valgrind for uninitialised reads |
| 18 | Production crashes: optimised, stripped, logs only | crash lab 15–16, `DEBUGGING.md` §6 | keep `.debug` files per release; install a failure signal handler |
| 19 | Build Gringofts (Docker), run its tests under ASan/TSan | `GRINGOFTS_GUIDE.md` G1 | check what flags production really uses (`DEBUGGING.md` §6.5) |
| 20 | Build/link errors and dependency issues | `DEBUGGING.md` §7 | `nm`, `c++filt`, `ldd`, `make VERBOSE=1` |

## Week 5 — Gringofts itself (Raft stays a black box)

| Day | Topic | Where |
|---|---|---|
| 21 | Capstone: mini event-sourced app + new command | `12_event_sourcing_capstone` |
| 22 | Protobuf & gRPC (async server, `CompletionQueue`, `CallData` lifetime) | `GRINGOFTS_GUIDE.md` G2 |
| 23 | Trace a request end to end; the shutdown order in `App::shutdown()` | G3 |
| 24–25 | First real change: add `DecreaseCommand` to `app_demo` with tests | G4 |
| later | Config + metrics changes; unit tests for infra | G5–G6 |

**Treating Raft as a black box** means knowing only its contract, as seen by the application:
commands go into `RaftCommandEventStore`, which calls `onPersisted()` once they're committed or
`onPersistFailed(code, msg, leaderHint)` if not (for example, this node isn't the leader). Committed events come back in
order through the apply loop, and every replica applies the same sequence. Your code must be
deterministic in `apply` and must handle the "not leader" reply. Open `src/infra/raft/` only when a
bug trace leads you there (G7).

## Week 6+: beyond the roadmap
See [`NEXT_STEPS.md`](NEXT_STEPS.md): a prioritised list of what to learn next (build, protobuf
compatibility, async gRPC, durability, performance, memory model), with exercises 13–17 and guided tasks G8–G10.

---

## Debugging
The full playbook is in [`DEBUGGING.md`](DEBUGGING.md): symptom tables, core dump setup (local,
systemd, Docker/k8s), gdb, sanitizers, recipes for hangs, leaks and Heisenbugs, and production
build hygiene. Practise with [`crash_lab/`](crash_lab/README.md).

## Books & references (in priority order)
1. *A Tour of C++* (3rd ed.), Stroustrup — 250 pages, the fastest overview for an experienced programmer.
2. *Effective Modern C++*, Scott Meyers — items on move semantics, smart pointers, lambdas, concurrency. Exactly the C++11–17 subset Gringofts uses.
3. [cppreference.com](https://en.cppreference.com) — your Javadoc. Check the "since C++XX" tags against C++17.
4. [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) — sections R (resources), C (classes), CP (concurrency).
5. [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) — what cpplint in Gringofts enforces.
6. *C++ Concurrency in Action* (2nd ed.), Anthony Williams — when you get to Raft internals.
7. [gRPC C++ async tutorial](https://grpc.io/docs/languages/cpp/async/) — before touching `RequestReceiver`.
