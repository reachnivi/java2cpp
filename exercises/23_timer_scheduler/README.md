# 23 — A timer scheduler: election timeouts, heartbeats, testable time

**Time:** 2.5 h · **Part 3.** Raft runs on timers: an election timeout that every heartbeat
pushes back, periodic heartbeats from the leader, request deadlines. Even with Raft as a black box,
you'll write timers for retries, leases and metrics, and timer code is notoriously hard to test.

## Design: separate the logic from the clock
- **`TimerQueue`**: pure, single-threaded logic. *You pass "now" in*, so tests use fake time and are
  100% deterministic (no sleeps, no flakes). A min-heap ordered by deadline, plus a map id → callback.
- **`TimerThread`**: a thin layer that drives a `TimerQueue` from a background thread with real time.

This "functional core, imperative shell" split is the single most useful testing idea for time-based code.
(It's the same idea as the injectable `now` in ex 15's `FakeServer`.)

## Details that matter
- `std::priority_queue` is a **max**-heap: your `operator<` must be inverted to get the earliest on top.
  Break ties with a sequence number so equal deadlines fire in scheduling order.
- **Lazy cancellation**: `priority_queue` can't remove from the middle. Drop the callback, and skip the
  stale heap entry when it reaches the top.
- **Wait with `condition_variable::wait_until(next)`, never `sleep`**: a newly scheduled *earlier* timer, or
  `stop()`, must wake the thread (ex 10's lesson again).
- **Run callbacks without holding the lock**: a callback that schedules a timer would otherwise deadlock
  (crash lab 09), and a slow callback would delay every other timer.
- Use `steady_clock`: the wall clock jumps with NTP; a timeout must not.

## Tasks (`src/timer.h`)
1. `Entry::operator<`, `schedule`, `cancel`, `reschedule`, `popDue`, `nextDeadline`.
2. `TimerThread::loop` (`stop()`, `scheduleAfter` etc. are given).

## Experiments
- Make the comparator `when < o.when` and run the order test. Explain the output.
- Replace `wait_until` with `sleep_until(next)` and run `EarlierTimerWakesTheSleepingThread`.
- Run callbacks while still holding the lock, then run `CallbackCanScheduleOnSameThread` (it times out
  after 3 s instead of hanging forever, because every wait in the test has a deadline).
- Production timer libraries use **hierarchical timing wheels** (O(1) insert/cancel) instead of a heap. Read about
  them (Varghese & Lauck, 1987) and sketch when you'd switch.
