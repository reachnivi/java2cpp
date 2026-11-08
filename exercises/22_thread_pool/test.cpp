#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "thread_pool.h"

using j2c::ThreadPool;
using namespace std::chrono_literals;  // NOLINT

namespace {
// Waits on a future with a deadline so a broken pool fails instead of hanging.
template <typename T>
bool readyWithin(std::future<T> &f, std::chrono::milliseconds d = 5000ms) {
  return f.valid() && f.wait_for(d) == std::future_status::ready;
}
}  // namespace

TEST(ThreadPoolTest, ZeroThreadsRejected) { EXPECT_THROW(ThreadPool(0), std::invalid_argument); }

TEST(ThreadPoolTest, ReturnsResults) {
  ThreadPool pool(4);
  auto f = pool.submit([] { return 6 * 7; });
  auto g = pool.submit([](int a, std::string b) { return b + std::to_string(a); }, 1, std::string("cmd-"));
  ASSERT_TRUE(readyWithin(f));
  ASSERT_TRUE(readyWithin(g));
  EXPECT_EQ(f.get(), 42);
  EXPECT_EQ(g.get(), "cmd-1");
}

TEST(ThreadPoolTest, VoidTasksAndManyResults) {
  ThreadPool pool(4);
  std::atomic<int> counter{0};
  std::vector<std::future<void>> fs;
  for (int i = 0; i < 1000; ++i) {
    fs.push_back(pool.submit([&counter] { ++counter; }));
  }
  for (auto &f : fs) {
    ASSERT_TRUE(readyWithin(f));
    f.get();
  }
  EXPECT_EQ(counter.load(), 1000);
}

TEST(ThreadPoolTest, ExceptionsTravelThroughTheFuture) {
  ThreadPool pool(2);
  auto f = pool.submit([]() -> int { throw std::runtime_error("handler failed"); });
  ASSERT_TRUE(readyWithin(f));
  EXPECT_THROW(f.get(), std::runtime_error);
  // The worker survived: the pool still works.
  auto g = pool.submit([] { return 1; });
  ASSERT_TRUE(readyWithin(g));
  EXPECT_EQ(g.get(), 1);
}

TEST(ThreadPoolTest, RunsTasksInParallel) {
  ThreadPool pool(4);
  std::mutex m;
  std::set<std::thread::id> ids;
  std::atomic<int> arrived{0};
  std::vector<std::future<void>> fs;
  for (int i = 0; i < 4; ++i) {
    fs.push_back(pool.submit([&] {
      {
        std::lock_guard<std::mutex> lock(m);
        ids.insert(std::this_thread::get_id());
      }
      ++arrived;
      // A barrier: only passes if all 4 tasks run at the same time.
      auto deadline = std::chrono::steady_clock::now() + 3s;
      while (arrived.load() < 4 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
      }
    }));
  }
  for (auto &f : fs) {
    ASSERT_TRUE(readyWithin(f));
  }
  EXPECT_EQ(arrived.load(), 4);
  EXPECT_EQ(ids.size(), 4u) << "4 workers should run 4 blocking tasks concurrently";
}

TEST(ThreadPoolTest, ShutdownDrainsQueuedWork) {
  std::atomic<int> done{0};
  {
    ThreadPool pool(1);
    for (int i = 0; i < 50; ++i) {
      pool.submit([&done] {
        std::this_thread::sleep_for(1ms);
        ++done;
      });
    }
    pool.shutdown();
    EXPECT_EQ(done.load(), 50) << "shutdown() must finish queued tasks before returning";
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
    pool.shutdown();  // idempotent
  }
}

TEST(ThreadPoolTest, DestructorJoinsWithoutShutdownCall) {
  std::atomic<int> done{0};
  {
    ThreadPool pool(2);
    for (int i = 0; i < 10; ++i) {
      pool.submit([&done] { ++done; });
    }
  }  // ~ThreadPool must drain and join (a joinable std::thread here = std::terminate)
  EXPECT_EQ(done.load(), 10);
}

TEST(ThreadPoolTest, TaskCanSubmitTask) {
  ThreadPool pool(2);
  auto outer = pool.submit([&pool] {
    auto inner = pool.submit([] { return 5; });
    return inner;  // hand the inner future back instead of blocking a worker on it
  });
  ASSERT_TRUE(readyWithin(outer));
  auto inner = outer.get();
  ASSERT_TRUE(readyWithin(inner));
  EXPECT_EQ(inner.get(), 5);
}

TEST(ThreadPoolTest, MoveOnlyArgumentsAndCaptures) {
  ThreadPool pool(2);
  auto f = pool.submit([](std::unique_ptr<int> p) { return *p + 1; }, std::make_unique<int>(41));
  ASSERT_TRUE(readyWithin(f));
  EXPECT_EQ(f.get(), 42);
}
