# What to learn next (after the roadmap)

You have the language core (exercises 00–12), debugging (`crash_lab/`, `DEBUGGING.md`) and a map
of Gringofts. What's left is mostly **what surrounds the code**: the build, the formats you persist,
the async RPC model, the disk, performance, and tooling. It's ordered by how soon it will bite you.

Each topic lists: **why** it matters for Gringofts · **where** it appears · the **hands-on** piece in this
repo (or a guided task in `GRINGOFTS_GUIDE.md` G8–G10) · the best **resource**.

---

## Tier 1: needed for almost any change

### 1. CMake and the Gringofts build, properly
- **Why:** every new file, test, proto, or dependency touches CMake. Build breakage is the first wall you'll hit.
- **Where:** root `CMakeLists.txt`, `src/*/CMakeLists.txt`, `test/CMakeLists.txt` (tests are **listed explicitly**),
  `third_party/` git submodules, `hooks/pre-commit-build`, protoc/grpc codegen rules.
- **Learn:** targets and `target_link_libraries` / `target_include_directories` (PUBLIC vs PRIVATE),
  `add_custom_command` for codegen, `find_package` vs `add_subdirectory`, build types and how
  `CMAKE_CXX_FLAGS_<CONFIG>` combine (see `DEBUGGING.md` §6.5 for why this matters in Gringofts),
  Ninja + `ccache` for fast rebuilds, `compile_commands.json` for your IDE.
- **Hands-on:** read this repo's `CMakeLists.txt` + `crash_lab/CMakeLists.txt` (globs, per-target flags, a post-build step).
- **Resource:** *Professional CMake* (Craig Scott), or the free "An Introduction to Modern CMake" (cliutils.gitlab.io/modern-cmake).

### 2. Protobuf and persisted-format compatibility
- **Why:** command/event payloads are protobuf bytes stored in the Raft log and snapshots. A careless
  schema change breaks replay, rolling upgrades or rollbacks. That's a data-loss-class bug.
- **Where:** `demo.proto`, `store.proto`, `raft.proto`, type constants in `domain/common_types.h`.
- **Hands-on:** **exercise 14** (`14_wire_compat`): implement the wire format; old/new reader compatibility; unknown-field preservation. Then G8 (evolve `demo.proto` for real, with golden-bytes tests).
- **Resource:** protobuf.dev → "Encoding" and "Proto Best Practices" (the "don't" list).

### 3. gRPC's async model
- **Why:** Gringofts' request path, leader forwarding and replication all use async gRPC. Leaks,
  shutdown crashes and hung requests come from getting the tag/lifetime rules wrong.
- **Where:** `app_util/RequestCallData.h`, `RequestReceiver`, `infra/forward/*`, deadline settings.
- **Learn:** `CompletionQueue` + tags, `CallData` state machines, status codes (`UNAVAILABLE`,
  `DEADLINE_EXCEEDED`), deadlines on every client call, channel/stub reuse, shutdown order, keepalive.
- **Hands-on:** **exercise 15** (`15_async_cq`). Then G9 (audit deadlines and statuses on real client calls).
- **Resource:** grpc.io → C++ "Asynchronous-API tutorial"; the `helloworld/greeter_async_*` examples in the gRPC repo.

### 4. Tooling you should run every day
- **Why:** it's much cheaper to find bugs before running, and reviewers will expect clean lint.
- **Learn:** clangd (VS Code) or CLion reading `compile_commands.json`; `clang-format`; `clang-tidy`;
  cpplint (`make gringofts_check`); coverage (`gcov`/`lcov`, which Gringofts' hooks already produce);
  Clang thread-safety annotations (`ABSL_GUARDED_BY`).
- **Hands-on:** `scripts/lint.sh` + the static-analysis bonus in `crash_lab/ANSWERS.md` (what clang-tidy
  catches: 1 of 16 bugs; what thread-safety annotations add).
- **Resource:** clang-tidy check list (clang.llvm.org/extra/clang-tidy/checks/list.html).

---

## Tier 2: running it in production

### 5. Linux file I/O, durability, and storage engines
- **Why:** "committed" means "on disk". Torn writes, missing fsyncs and full disks are how
  replicated systems lose data. Gringofts stores state in segment files, RocksDB and SQLite.
- **Where:** `raft/storage/Segment.cpp` (mmap), `util/FileUtil`, `app_demo/v2/RocksDBBackedAppStateMachine`,
  `es/store/SQLite*`.
- **Learn:** `write` vs `fsync`/`fdatasync`, directory fsync, `O_APPEND`, short writes, `errno` →
  `std::system_error`, mmap and SIGBUS, `ENOSPC` handling. RocksDB: `WriteBatch` (atomic multi-key
  writes), `WriteOptions::sync`, `Status` checking, column families, compaction and write stalls.
- **Hands-on:** **exercise 13** (`13_durable_log`). Then G10 (RocksDB WriteBatch in the v2 state machine).
- **Resource:** "Files are hard" (Dan Luu); *The Linux Programming Interface* (Kerrisk), ch. 4–5, 13, 49;
  RocksDB wiki "Basic Operations" + "RocksDB Tuning Guide".

### 6. Time and clocks
- **Why:** timeouts, leases, election timers and latency metrics all depend on picking the right clock.
- **Learn:** `steady_clock` for durations and deadlines; `system_clock` for timestamps (event metadata);
  never mix them; injectable clocks for testable timing (ex 15's `FakeServer`).
- **Where:** `util/TimeUtil.h`, `Command` timestamps (`TimestampInNanos`), metrics latency reporting.

### 7. Observability
- **Why:** you'll debug production mostly from metrics and logs, not gdb.
- **Where:** `infra/monitor/` (`santiago/` = prometheus-cpp wrapper, `getCounter`/`getGauge` in
  `MonitorTypes.h`), `MetricReporter.h` (latency histograms in `Command::reportMetrics()`), spdlog pattern in `Main.cpp`.
- **Learn:** counter vs gauge vs histogram; label cardinality (never label by request id); spdlog sinks,
  async logging and flush policy (`DEBUGGING.md` §5); the RED method (rate, errors, duration) per RPC.
- **Hands-on:** G6 (add a rejected-command counter).

### 8. TLS and crypto basics
- **Why:** Gringofts can encrypt persisted data (`Crypto` is used by `RaftCommandEventStore` and for
  snapshot files) and uses TLS for gRPC. Misconfiguring either is an outage or a breach.
- **Where:** `util/TlsUtil`, `util/CryptoUtil`, `SecretKey*`, `es/Crypto.h` (~30 OpenSSL `EVP_`/AES uses).
- **Learn:** what `EVP_` AEAD encryption needs (unique IV/nonce per message, auth tag), key rotation
  and versioned keys (old log entries must stay decryptable), cert/CA configuration for gRPC.
- **Resource:** *Serious Cryptography* (Aumasson), ch. 4 and 8; OpenSSL EVP docs.

---

## Tier 3: performance

### 9. Allocation, copies and profiling
- **Why:** the throughput target (thousands of tx/s per cluster) is spent on allocations, copies, locks and I/O.
- **Where:** `util/MemoryPool`, `TrackingMemoryResource`, `PMRContainerFactory` (Gringofts' `std::pmr` setup).
- **Hands-on:** **exercise 16** (`16_pmr_perf`), including the `perf record` / flame-graph walkthrough.
- **Resource:** Brendan Gregg's perf and flame-graph pages; *Optimized C++* (Kurt Guntheroth);
  CppCon talks "Want fast C++? Know your hardware" (Timur Doumler), and Chandler Carruth's talks on benchmarking.

### 10. The memory model and lock-free code
- **Why:** you'll review `std::atomic` code and must know when relaxed ordering is a bug.
- **Hands-on:** **exercise 17** (`17_lockfree_spsc`): acquire/release publication, false sharing,
  and TSan catching a "works on x86" ordering bug.
- **Resource:** *C++ Concurrency in Action* (Williams), ch. 5 and 7; Herb Sutter's "atomic<> Weapons" talk.

---

## Tier 4: distributed-systems literacy (Raft stays a black box)
You don't need Raft internals, but you do need the guarantees your application code relies on:
- **Determinism:** `apply` must give the same result on every replica and on every replay (no clocks, randomness or I/O).
- **Idempotency and dedup:** clients retry, and leaders change mid-request. See `Command::dedupId()` and the
  "value must be state + 1" pattern in `IncreaseHandler` (201 = duplicate).
- **Not-leader handling:** `onPersistFailed(code, msg, leaderHint)`; forwarding in `infra/forward/`.
- **Snapshots:** state machine save/restore and compatibility (`es/store/SnapshotUtil.h`).
- **Reconfiguration and split:** `app_util/control/reconfigure`, `control/split`: how the cluster changes shape at run time.
- **Resource:** *Designing Data-Intensive Applications* (Kleppmann), ch. 5, 7, 9 (replication, transactions,
  consistency). It's the single best book for this layer.

---

## Language features you'll meet but we haven't drilled
| Topic | Where it shows up | Learn |
|---|---|---|
| Exception-safety guarantees (basic / strong / nothrow), `noexcept` | constructors, containers of your types | Meyers *Effective Modern C++* items 14, 17; ex 03's copy-and-swap |
| `std::variant` + `std::visit` | alternative to type-switches over events | cppreference; "type-safe unions" |
| Reading `enable_if` / SFINAE / CRTP | third-party headers (abseil, gRPC, boost) | *C++ Templates: The Complete Guide* ch. 8, 21 (read to understand, don't write it) |
| `constexpr` and `static_assert` | compile-time checks on sizes and layouts (crash lab 07's fix) | cppreference |
| Move-only callbacks | `std::function` requires copyable captures; capturing a `unique_ptr` fails | know the error; wrap in `shared_ptr` or use a custom move-only function |
| `std::chrono` arithmetic | timeouts, metrics | Howard Hinnant's chrono tutorial (CppCon 2016) |

---

## Suggested order (about 4 more weeks, same pace as the roadmap)
| Week | Focus |
|---|---|
| 6 | ex 14 (wire compat) → G8; CMake reading; set up clangd/CLion + `scripts/lint.sh` |
| 7 | ex 15 (async CQ) → G9; ex 13 (durable log) → read `Segment.cpp`, G10 |
| 8 | ex 16 (pmr/perf) + profile the Gringofts demo under load; observability: G6 |
| 9 | ex 17 (memory model); Tier 4 reading (DDIA ch. 5 and 9); pick a real ticket |
