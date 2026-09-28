#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>

#include "mpsc_queue.h"

using j2c::BlockingMpscQueue;
using j2c::QueueStoppedException;

TEST(MpscQueue, FifoSingleThread) {
  BlockingMpscQueue<int> q;
  EXPECT_TRUE(q.empty());
  q.enqueue(1);
  q.enqueue(2);
  q.enqueue(3);
  EXPECT_EQ(q.size(), 3u);
  EXPECT_EQ(q.dequeue(), 1);
  EXPECT_EQ(q.dequeue(), 2);
  EXPECT_EQ(q.dequeue(), 3);
  EXPECT_TRUE(q.empty());
}

TEST(MpscQueue, DequeueBlocksUntilItemArrives) {
  BlockingMpscQueue<int> q;
  std::atomic<bool> got{false};
  std::thread consumer([&] {
    // An exception escaping a std::thread calls std::terminate(), so catch here.
    try {
      EXPECT_EQ(q.dequeue(), 42);
      got = true;
    } catch (const std::exception &e) {
      ADD_FAILURE() << "dequeue threw: " << e.what();
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(got.load()) << "dequeue() must block on an empty queue";
  q.enqueue(42);
  consumer.join();
  EXPECT_TRUE(got.load());
}

TEST(MpscQueue, ManyProducersOneConsumer) {
  BlockingMpscQueue<int> q;
  constexpr int kProducers = 8;
  constexpr int kPerProducer = 5000;
  std::vector<std::thread> producers;
  for (int p = 0; p < kProducers; ++p) {
    producers.emplace_back([&q, p] {
      for (int i = 0; i < kPerProducer; ++i) {
        q.enqueue(p * kPerProducer + i);
      }
    });
  }
  long long sum = 0;
  try {
    for (int i = 0; i < kProducers * kPerProducer; ++i) {
      sum += q.dequeue();
    }
  } catch (const std::exception &e) {
    ADD_FAILURE() << "dequeue threw: " << e.what();
  }
  // Must join before the std::thread objects are destroyed, even on failure.
  for (auto &t : producers) {
    t.join();
  }
  long long n = static_cast<long long>(kProducers) * kPerProducer;
  EXPECT_EQ(sum, n * (n - 1) / 2);
  EXPECT_TRUE(q.empty());
}

TEST(MpscQueue, ShutdownWakesBlockedConsumer) {
  BlockingMpscQueue<int> q;
  std::atomic<bool> threw{false};
  std::thread consumer([&] {
    try {
      q.dequeue();
    } catch (const QueueStoppedException &) {
      threw = true;
    } catch (const std::exception &e) {
      ADD_FAILURE() << "unexpected exception: " << e.what();
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(threw.load()) << "dequeue() must block until shutdown()";
  q.shutdown();
  consumer.join();
  EXPECT_TRUE(threw.load());
}

TEST(MpscQueue, ShutdownDrainsRemainingThenThrows) {
  BlockingMpscQueue<int> q;
  q.enqueue(1);
  q.enqueue(2);
  q.shutdown();
  EXPECT_THROW(q.enqueue(3), QueueStoppedException);
  EXPECT_EQ(q.dequeue(), 1);
  EXPECT_EQ(q.dequeue(), 2);
  EXPECT_THROW(q.dequeue(), QueueStoppedException);
}

TEST(MpscQueue, WorksWithMoveOnlyPayloadsViaSharedPtr) {
  // Gringofts queues std::shared_ptr<Command>; do the same.
  BlockingMpscQueue<std::shared_ptr<std::string>> q;
  q.enqueue(std::make_shared<std::string>("cmd"));
  auto p = q.dequeue();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(*p, "cmd");
}
