#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "spsc.h"

using j2c::lockfree::SpscRing;

TEST(Spsc, IndicesAreLockFree) {
  EXPECT_TRUE(std::atomic<size_t>::is_always_lock_free);
}

TEST(Spsc, FifoAndCapacity) {
  SpscRing<int, 4> q;
  EXPECT_EQ(q.tryPop(), std::nullopt);
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(q.tryPush(i));
  }
  EXPECT_FALSE(q.tryPush(99)) << "full at capacity N";
  EXPECT_EQ(q.sizeApprox(), 4u);
  EXPECT_EQ(q.tryPop(), 0);
  EXPECT_TRUE(q.tryPush(4));  // wraps around
  for (int i = 1; i <= 4; ++i) {
    EXPECT_EQ(q.tryPop(), i);
  }
  EXPECT_EQ(q.tryPop(), std::nullopt);
}

TEST(Spsc, MoveOnlyPayload) {
  SpscRing<std::unique_ptr<std::string>, 2> q;
  EXPECT_TRUE(q.tryPush(std::make_unique<std::string>("cmd")));
  auto p = q.tryPop();
  ASSERT_TRUE(p.has_value());
  ASSERT_NE(*p, nullptr);
  EXPECT_EQ(**p, "cmd");
}

TEST(Spsc, ProducerConsumerStressKeepsOrder) {
  // Run the ThreadSanitizer build of this test too: it must be race-free.
  constexpr uint64_t kCount = 500000;
  SpscRing<uint64_t, 1024> q;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  std::atomic<bool> timedOut{false};

  std::thread producer([&] {
    for (uint64_t i = 1; i <= kCount;) {
      if (q.tryPush(i)) {
        ++i;
      } else if (std::chrono::steady_clock::now() > deadline) {
        timedOut = true;
        return;
      } else {
        std::this_thread::yield();
      }
    }
  });

  uint64_t expected = 1;
  uint64_t sum = 0;
  while (expected <= kCount && !timedOut) {
    if (auto v = q.tryPop()) {
      if (*v != expected) {
        ADD_FAILURE() << "out of order: got " << *v << " expected " << expected;
        break;
      }
      sum += *v;
      ++expected;
    } else if (std::chrono::steady_clock::now() > deadline) {
      timedOut = true;
    } else {
      std::this_thread::yield();
    }
  }
  producer.join();
  EXPECT_FALSE(timedOut.load()) << "stalled at " << expected;
  EXPECT_EQ(sum, kCount * (kCount + 1) / 2);
}
