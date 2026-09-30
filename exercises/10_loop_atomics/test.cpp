#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "loop.h"

using j2c::Metrics;
using j2c::Worker;
using namespace std::chrono_literals;  // NOLINT

TEST(Atomics, ConcurrentCounterIsExact) {
  Metrics m;
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&m] {
      for (int i = 0; i < 20000; ++i) {
        m.recordProcessed();
      }
    });
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(m.processed(), 160000u);
}

TEST(Worker, TicksUntilStopped) {
  std::atomic<int> ticks{0};
  Worker w;
  w.start([&ticks] { ++ticks; }, 1ms);
  EXPECT_TRUE(w.isRunning());
  std::this_thread::sleep_for(50ms);
  w.stop();
  EXPECT_FALSE(w.isRunning());
  int after = ticks.load();
  EXPECT_GT(after, 3);
  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(ticks.load(), after) << "worker kept ticking after stop()";
}

TEST(Worker, StopIsIdempotentAndDestructorJoins) {
  std::atomic<int> ticks{0};
  {
    Worker w;
    w.start([&ticks] { ++ticks; }, 1ms);
    std::this_thread::sleep_for(10ms);
    w.stop();
    w.stop();
  }
  {
    Worker w;
    w.start([&ticks] { ++ticks; }, 1ms);
    std::this_thread::sleep_for(10ms);
    // no stop(): destructor must join, otherwise std::terminate()
  }
  SUCCEED();
}

TEST(Worker, StartTwiceThrows) {
  Worker w;
  w.start([] {}, 1ms);
  EXPECT_THROW(w.start([] {}, 1ms), std::logic_error);
  w.stop();
}

TEST(Worker, StopIsPromptEvenWithLongInterval) {
  std::atomic<int> ticks{0};
  Worker w;
  w.start([&ticks] { ++ticks; }, 10s);
  std::this_thread::sleep_for(20ms);
  auto begin = std::chrono::steady_clock::now();
  w.stop();
  auto elapsed = std::chrono::steady_clock::now() - begin;
  EXPECT_LT(elapsed, 1s) << "stop() waited for the sleep to finish";
  EXPECT_EQ(ticks.load(), 1);
}
