# 10 — Atomics and thread-owning "Loop" objects

**Time:** 1.5 h · **Gringofts link:** `src/infra/es/Loop.h`, `src/app_util/CommandProcessLoop.h`,
`src/app_util/EventApplyLoop.h`, `App.h` (`std::thread mCommandProcessLoopThread` etc.),
`std::atomic<bool> mShouldExit` in `MpscDoubleBufferQueue.h`, `src/infra/util/Signal.h`.

Gringofts is structured as several long-running **loops**, each on its own
`std::thread`: receive requests → process commands → persist via Raft → apply
events. Each loop has a `run()` and a `shutdown()` and checks an
`std::atomic<bool>` flag. Getting start/stop/join right is where most
production crashes come from.

## Tasks (`src/loop.h`)
1. **`Metrics`** — `recordProcessed()` callable from many threads at once,
   `processed()` returns the total. Use `std::atomic<uint64_t>` and `fetch_add`.
   (Try a plain `uint64_t` first and run under `-DJ2C_SANITIZER=thread`.)
2. **`Worker`** — owns a `std::thread`.
   - `start(std::function<void()> tick, std::chrono::milliseconds interval)`:
     spawns a thread that calls `tick()` then sleeps `interval`, until stopped.
     Calling `start` twice throws `std::logic_error`.
   - `stop()`: sets the flag, **joins**. Idempotent (safe to call twice).
   - `isRunning()`.
   - destructor calls `stop()` — destroying a joinable `std::thread` calls
     `std::terminate()`. This is the #1 crash for Java devs in C++.
   - **Stop must be prompt:** even with a 10-second interval, `stop()` should
     return quickly. Sleeping with `std::this_thread::sleep_for` can't be
     interrupted — use `std::condition_variable::wait_for` with a predicate.

## Think about
- `std::atomic<bool>` vs `bool` guarded by a mutex: when do you need the mutex?
  (When the flag and some *other* state must change together, or you need to
  wait on it with a condition variable.)
- `memory_order_relaxed` is fine for a statistics counter. Stick to the default
  (`seq_cst`) everywhere else until you have a profiler telling you otherwise.
- In Gringofts' `App::shutdown()`, which order are the loops stopped in, and why?
