# Gringofts field guide: reading tasks & first contributions

Paths are relative to a clone of <https://github.com/eBay/Gringofts>.

## Codebase map
```
src/
├── infra/                  # the framework — most of your augmentation work lands here or in app_util
│   ├── es/                 # event sourcing core: Command, Event, StateMachine, CommandEventStore, Loop
│   │   └── store/          # store impls: Raft-backed, SQLite-backed, in-memory (Default*)
│   ├── raft/               # Raft consensus
│   │   ├── v2/RaftCore.*   # leader election, log replication (the heart; read last)
│   │   ├── storage/        # segmented on-disk Raft log
│   │   └── *.proto         # Raft RPC messages
│   ├── mpscqueue/          # MpscQueue<T> + double-buffer impl (you rebuilt this in ex 09)
│   ├── grpc/               # RequestHandle, TLS helpers
│   ├── monitor/            # prometheus metrics (santiago/), MonitorCenter
│   ├── forward/            # forward requests from followers to the leader
│   └── util/               # ClusterInfo, TimeUtil, FileUtil, CryptoUtil, IdGenerator, ...
├── app_util/               # reusable application scaffolding
│   ├── CommandProcessLoop.h    # template: pulls commands off the queue, runs handlers, persists
│   ├── EventApplyLoop.h        # applies committed events to the state machine
│   ├── RequestCallData.h       # async gRPC request state machine
│   └── AppStateMachine.h
└── app_demo/               # a complete example app: a replicated counter
    ├── AppStateMachine.*       # registers command processors & event appliers
    ├── execution/              # IncreaseHandler (process), IncreaseApplier (apply)
    ├── v2/                     # memory- and RocksDB-backed state machines
    └── should_be_generated/    # boilerplate you'd generate per app: commands, events, decoders, gRPC receiver, App
test/                       # gtest suite: ONE runner, files listed explicitly in test/CMakeLists.txt
conf/                       # *.ini configs for the demo (single node, 3-node cluster, SQLite)
examples/                   # run_demo_*.sh scripts
hooks/                      # pre-commit-build (cpplint + build), pre-commit-unittest
```

## G1 — Build it and run the tests (day 15)
Follow the README's Docker path — it pins the toolchain (g++-9) and all dependencies:
```bash
git clone https://github.com/eBay/Gringofts.git && cd Gringofts
bash ./scripts/addSubmodules.sh
sudo docker build --rm -t gringofts/dependencies:v3 -f dockers/dependencies/download.Dockerfile .
sudo docker build --rm -t gringofts/compile:v3 -f dockers/dependencies/install.Dockerfile .
sudo docker run --workdir "$(pwd)" --mount type=bind,source="$(pwd)",target="$(pwd)" \
     --user "$(id -u)":"$(id -g)" gringofts/compile:v3 hooks/pre-commit
```
Then:
- Run `examples/run_demo_backed_by_single_cluster.sh` and hit it with
  `grpc_cli call 0.0.0.0:50055 gringofts.demo.protos.DemoService.Execute "value:1"`
  (`value:2`, then `value:2` again → 201, then `value:9` → 400).
- Run the unit tests (`hooks/pre-commit-unittest`, or the `gringofts_TestRunner`
  binary directly with `--gtest_filter=MpscDoubleBufferQueue*`).
- Open `docs/C++ Development Environment.pdf` for the IDE setup (CLion works
  well for a Java/IntelliJ user — it reads CMake directly).

**Exercise:** find the generated `demo.pb.h` in the build tree. Skim it: this is what `protoc` makes from `demo.proto`.

## G2 — Protobuf & gRPC reading (day 17)
Read, in order:
1. `src/app_demo/should_be_generated/domain/protos/demo.proto` — one service, one rpc.
2. `IncreaseCommand.h` — `encodeToString()` is `mRequest.SerializeAsString()`; protobuf is the wire *and* the storage format.
3. `src/app_util/RequestCallData.h` + `app_demo/.../app/RequestReceiver.{h,cpp}` —
   the **async** gRPC server pattern: each in-flight RPC is a heap object
   (`CallData`) used as the `void *tag` on a `CompletionQueue`; a thread loops on
   `cq->Next(&tag, &ok)` and advances that object's state machine
   (CREATE → PROCESS → FINISH). Compare with how you'd write this in Java
   with `StreamObserver` — same idea, manual memory management.

**Exercise:** write down (on paper) the lifetime of one `CallData` object:
who `new`s it, which thread touches it when, and who `delete`s it.

## G3 — Trace one request end to end (day 18)
Follow `Execute(value:1)` through the code, noting the thread at each hop:
1. gRPC CQ thread: `RequestReceiver` → `CallDataHandler::buildCommand` → `std::make_shared<IncreaseCommand>`
2. enqueue into `BlockingQueue<std::shared_ptr<Command>> mCommandQueue` (`App.h`)
3. `CommandProcessLoop` thread (`app_util/CommandProcessLoop.h` + `domain/CommandProcessLoop.hpp`):
   dequeue → `verifyCommand()` → `AppStateMachine::processCommandAndApply` →
   `IncreaseHandler::process` emits `ProcessedEvent`
4. `CommandEventStore` (Raft-backed): the command+events are appended to the Raft log and replicated
5. once committed: `Command::onPersisted()` → reply sent via the saved `RequestHandle`
6. `EventApplyLoop` thread: `IncreaseApplier::apply` updates the (persisted) state machine
7. on restart / follower: events are replayed from the log (that's your `App::recover` from ex 12)

**Exercise:** in `App::shutdown()` (`App.cpp`) the queue is shut down first,
then the loops, then the store, then threads are joined. Explain why this
order avoids both deadlocks and use-after-free. (Hint: ex 09 + ex 10.)

## G4 — First feature: `DecreaseCommand` in `app_demo` (days 19–20)
You did this in miniature in ex 12. The real checklist:
1. **proto**: add `rpc Decrease (DecreaseRequest) returns (IncreaseResponse)` (or
   a new response type) and a `DecreaseRequest` message in `demo.proto`.
2. **types**: in `domain/common_types.h` add `DECREASE_COMMAND` and
   `DECREASED_EVENT` constants (unique values; they're persisted in the log — never reuse one).
3. **command/event classes**: `DecreaseCommand.{h,cpp}` and `DecreasedEvent.{h,cpp}`
   modelled on `IncreaseCommand` / `ProcessedEvent`.
4. **decoders**: new `case`s in `CommandDecoderImpl.cpp` and `EventDecoderImpl.cpp` —
   without these, recovery can't read your entries back.
5. **handler + applier**: `execution/DecreaseHandler.*`, `execution/DecreaseApplier.*`;
   register them in `AppStateMachine.cpp` next to the existing
   `registerCommandProcessor(INCREASE_COMMAND, ...)` / `registerEventApplier(PROCESSED_EVENT, ...)` lambdas;
   update the `v2/` state machines if they need a new method.
6. **gRPC receiver**: a second `CallDataHandler` for the new rpc, registered with the server.
7. **build**: add new `.cpp` files to `src/app_demo/CMakeLists.txt`.
8. **tests**: add a test file under `test/` **and list it in `test/CMakeLists.txt`**
   (the list is explicit; unlisted files silently don't run).
9. `hooks/pre-commit` must pass (cpplint + build).

## G5 — Add unit tests to existing infra (good first PRs)
Pick one; each is small and teaches a subsystem:
- `test/infra/mpscqueue/MpscDoubleBufferQueueTest.cpp`: add a
  multi-producer stress test like the one in ex 09, run it under TSan.
- `test/infra/util/`: find a util in `src/infra/util/` without a test (e.g. `StrUtil.h`) and add one.
- `test/infra/es/`: use `ReadonlyCommandEventStoreMock.h` (gmock) to test a
  failure path in a loop class.

## G6 — Config + metrics (operational features are common asks)
- Add a new key to `conf/app_raft_0.ini` and read it with
  `INIReader::Get/GetInteger` where the app is built (look for `INIReader` in
  `App.cpp` / `AppInfo.cpp`). Default it so old configs still work.
- Add a counter for rejected commands (HTTP-style code != 200) using
  `getCounter(name, labels)` from `src/infra/monitor/MonitorTypes.h`; see how
  existing metrics are named and labelled first.

## G7 — Raft internals (week 4+)
Read `src/infra/raft/v2/RaftCore.h` first (state, roles, the main loop), then
`RaftCore.cpp` alongside the [Raft paper](https://raft.github.io/raft.pdf)
section 5. Tests in `test/infra/raft/v2/RaftCoreTest.cpp` and `ClusterTestUtil.cpp` show
how multi-node clusters are simulated in-process. Don't modify Raft code
until you can explain how `commitIndex` advances and why the leader only
commits entries from its own term.

## Contribution hygiene
- Match the local style: `mMember`, `kConstant`, `SPDLOG_*` macros, header
  guards `SRC_PATH_FILE_H_`, `/// namespace foo` closing comments, 120-char lines.
- Run `make gringofts_check` (cpplint) before pushing.
- Anything that changes persisted formats (protobuf fields, type constants,
  RocksDB keys) must stay backward compatible with existing logs/snapshots:
  add fields, never renumber or reuse.
- Anything in `applyEvent` / appliers must be deterministic.
