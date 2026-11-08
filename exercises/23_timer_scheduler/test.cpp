#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "timer.h"

using j2c::Clock;
using j2c::TimePoint;
using j2c::TimerId;
using j2c::TimerQueue;
using j2c::TimerThread;
using namespace std::chrono_literals;  // NOLINT

// ---------------------------------------------------------------- TimerQueue
// Pure logic with fake time: fully deterministic.

TEST(TimerQueueTest, FiresInDeadlineOrderWithFifoTies) {
  TimerQueue q;
  TimePoint t0{};
  std::vector<std::string> log;
  q.schedule(t0 + 30ms, [&] { log.push_back("c"); });
  q.schedule(t0 + 10ms, [&] { log.push_back("a"); });
  q.schedule(t0 + 20ms, [&] { log.push_back("b1"); });
  q.schedule(t0 + 20ms, [&] { log.push_back("b2"); });
  EXPECT_EQ(q.runDue(t0 + 5ms), 0u);
  EXPECT_EQ(q.runDue(t0 + 20ms), 3u);
  EXPECT_EQ(q.runDue(t0 + 100ms), 1u);
  std::vector<std::string> expected{"a", "b1", "b2", "c"};
  EXPECT_EQ(log, expected);
  EXPECT_EQ(q.pending(), 0u);
}

TEST(TimerQueueTest, CancelPreventsFiring) {
  TimerQueue q;
  TimePoint t0{};
  int fired = 0;
  TimerId a = q.schedule(t0 + 10ms, [&] { ++fired; });
  q.schedule(t0 + 10ms, [&] { fired += 10; });
  EXPECT_TRUE(q.cancel(a));
  EXPECT_FALSE(q.cancel(a)) << "second cancel: already gone";
  EXPECT_EQ(q.runDue(t0 + 10ms), 1u);
  EXPECT_EQ(fired, 10);
  EXPECT_FALSE(q.cancel(12345));
}

TEST(TimerQueueTest, ElectionTimeoutResetOnHeartbeat) {
  // Raft-style: a follower's election timer is pushed back on every heartbeat.
  TimerQueue q;
  TimePoint t0{};
  bool electionStarted = false;
  TimerId election = q.schedule(t0 + 150ms, [&] { electionStarted = true; });
  for (int beat = 1; beat <= 5; ++beat) {
    TimePoint now = t0 + beat * 100ms;
    q.runDue(now);
    election = q.reschedule(election, now + 150ms);
    ASSERT_NE(election, 0u) << "timer must still be alive at heartbeat " << beat;
  }
  EXPECT_FALSE(electionStarted) << "heartbeats every 100ms keep a 150ms timeout from firing";
  q.runDue(t0 + 500ms + 149ms);
  EXPECT_FALSE(electionStarted);
  q.runDue(t0 + 500ms + 150ms);
  EXPECT_TRUE(electionStarted) << "heartbeats stopped: the election starts";
  EXPECT_EQ(q.reschedule(election, t0 + 1s), 0u) << "can't reschedule a fired timer";
}

TEST(TimerQueueTest, CallbacksMaySchedule) {
  TimerQueue q;
  TimePoint t0{};
  int ticks = 0;
  std::function<void()> tick;
  TimePoint now = t0;
  tick = [&] {
    ++ticks;
    q.schedule(now + 10ms, tick);  // periodic timer re-arms itself
  };
  q.schedule(t0 + 10ms, tick);
  for (int i = 1; i <= 5; ++i) {
    now = t0 + i * 10ms;
    EXPECT_EQ(q.runDue(now), 1u) << "re-armed timer must fire once per period, not loop forever";
  }
  EXPECT_EQ(ticks, 5);
}

TEST(TimerQueueTest, NextDeadlineSkipsCancelled) {
  TimerQueue q;
  TimePoint t0{};
  TimePoint next;
  EXPECT_FALSE(q.nextDeadline(&next));
  TimerId a = q.schedule(t0 + 5ms, [] {});
  q.schedule(t0 + 9ms, [] {});
  q.cancel(a);
  ASSERT_TRUE(q.nextDeadline(&next));
  EXPECT_EQ(next, t0 + 9ms);
}

// --------------------------------------------------------------- TimerThread
// Real time: generous tolerances, and every wait has a deadline.

namespace {
class Latch {
 public:
  void countDown() {
    std::lock_guard<std::mutex> l(mM);
    ++mCount;
    mCv.notify_all();
  }
  bool waitFor(int n, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> l(mM);
    return mCv.wait_for(l, timeout, [&] { return mCount >= n; });
  }
  int count() {
    std::lock_guard<std::mutex> l(mM);
    return mCount;
  }

 private:
  std::mutex mM;
  std::condition_variable mCv;
  int mCount = 0;
};
}  // namespace

TEST(TimerThreadTest, FiresAfterDelay) {
  TimerThread t;
  Latch latch;
  auto start = Clock::now();
  std::atomic<long long> elapsedMs{-1};
  t.scheduleAfter(50ms, [&] {
    elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
    latch.countDown();
  });
  ASSERT_TRUE(latch.waitFor(1, 3s)) << "timer never fired";
  EXPECT_GE(elapsedMs.load(), 50);
  EXPECT_LT(elapsedMs.load(), 1000);
}

TEST(TimerThreadTest, EarlierTimerWakesTheSleepingThread) {
  TimerThread t;
  Latch latch;
  t.scheduleAfter(10s, [] {});  // the thread is now sleeping until +10s
  auto start = Clock::now();
  t.scheduleAfter(20ms, [&] { latch.countDown(); });
  ASSERT_TRUE(latch.waitFor(1, 3s)) << "a new earlier timer must wake the thread";
  EXPECT_LT(Clock::now() - start, 2s);
}

TEST(TimerThreadTest, CancelAndReset) {
  TimerThread t;
  Latch latch;
  std::atomic<int> cancelledFired{0};
  TimerId a = t.scheduleAfter(30ms, [&] { ++cancelledFired; });
  EXPECT_TRUE(t.cancel(a));
  TimerId b = t.scheduleAfter(30ms, [&] { latch.countDown(); });
  TimerId b2 = t.resetAfter(b, 60ms);
  EXPECT_NE(b2, 0u);
  ASSERT_TRUE(latch.waitFor(1, 3s));
  EXPECT_EQ(cancelledFired.load(), 0);
}

TEST(TimerThreadTest, CallbackCanScheduleOnSameThread) {
  TimerThread t;
  Latch latch;
  t.scheduleAfter(10ms, [&] {
    latch.countDown();
    t.scheduleAfter(10ms, [&] { latch.countDown(); });  // deadlocks if callbacks run under the lock
  });
  EXPECT_TRUE(latch.waitFor(2, 3s));
}

TEST(TimerThreadTest, StopIsPromptAndDropsPendingTimers) {
  std::atomic<int> fired{0};
  auto start = Clock::now();
  {
    TimerThread t;
    t.scheduleAfter(10s, [&] { ++fired; });
    std::this_thread::sleep_for(20ms);
  }  // destructor stops and joins
  EXPECT_LT(Clock::now() - start, 2s) << "stop() must not wait for the 10s timer";
  EXPECT_EQ(fired.load(), 0);
}
