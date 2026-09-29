# 15 — The gRPC async server model: completion queues, CallData, deadlines

**Time:** 2.5 h · **Gringofts link:** `src/app_util/RequestCallData.h` (the CREATE → PROCESS →
FINISH state machine), `src/app_demo/should_be_generated/app/RequestReceiver.{h,cpp}`,
`src/app_util/RequestReceiver.h` (the thread that loops on `cq->Next`), `infra/forward/`
(async *client* calls to the leader), and the ~10 places that set deadlines.

## Why
Gringofts doesn't use the thread-per-request synchronous gRPC API. It uses the **async** API: a few
threads loop on a `CompletionQueue`, and each in-flight RPC is a heap object (`CallData`) whose
address is the `void *tag`. It's fast and it's where the subtle bugs live: leaks, use-after-free
on shutdown, calls that never complete. Java's `StreamObserver` hides this. Here you build the
model yourself, with no gRPC dependency. `FakeServer` stands in for `grpc::Server` + the generated service.

```
 client ──inject()──► FakeServer ──post(tag, ok)──► CompletionQueue ──next()──► loop thread
                          ▲                                                        │
                          └────── requestCall(&req, this) / finish(resp, this) ◄───┘ CallData::proceed(ok)
```

## Rules of the real API (they're what the tests check)
- A tag comes back from `next()` **exactly once** per operation you started (`requestCall`, `finish`).
- `ok == false` means the operation did not happen (for example, the server shut down before a request
  arrived). The object must clean itself up.
- In PROCESS, **create the next `CallData` first**. Otherwise the server stops accepting requests after the first.
- `CallData` deletes itself (`delete this`) in FINISH. Its destructor is private so nobody else can.
- Shutdown order: `server->Shutdown()` (pending calls complete with `ok=false`, in-flight calls finish),
  then `cq->Shutdown()`, then **keep draining `next()` until it returns false**. Then destroy the CQ.
  Skipping the drain leaks every `CallData` and trips gRPC assertions.
- **Deadlines** are absolute points on `std::chrono::steady_clock`. Never use `system_clock` for
  timeouts: it jumps when NTP adjusts the wall clock. (`system_clock` is right for *timestamps*, like
  Gringofts' `TimeUtil::currentTimeInNanos()` in event metadata.)

## Tasks (`src/async_cq.h`)
1. `CompletionQueue::post / next / shutdown` (ex 09 skills: mutex, condvar, predicate wait, drain on shutdown).
2. `CallData::proceed(bool ok)`: the state machine described in the comments.

## Experiments
- Remove the `new CallData(...)` in PROCESS: which test fails, and how?
- Remove the `!ok` branch. What does the `CallData` now do with a request that never arrived,
  and which test catches it? (In real gRPC this is how bogus responses and use-after-free on shutdown happen.)
- Why does `FakeServer` take `now` as a `std::function`? (Injectable clocks make time-dependent code testable. See the deadline test.)
- Read `RequestCallData.h` and find where Gringofts does each of the steps above.
