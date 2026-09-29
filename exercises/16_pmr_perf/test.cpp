#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <memory_resource>
#include <string_view>

#include "pmr.h"

using namespace j2c::mem;  // NOLINT

TEST(CountingResourceTest, CountsAndBalances) {
  CountingResource counting;
  {
    std::pmr::vector<int> v(&counting);
    v.push_back(1);
    EXPECT_EQ(counting.allocations(), 1u);
    EXPECT_GE(counting.bytesOutstanding(), sizeof(int));
  }
  EXPECT_EQ(counting.deallocations(), counting.allocations());
  EXPECT_EQ(counting.bytesOutstanding(), 0u);
  EXPECT_GT(counting.peakBytes(), 0u);
}

TEST(CountingResourceTest, OnlyEqualToItself) {
  CountingResource a, b;
  EXPECT_TRUE(a.is_equal(a));
  EXPECT_FALSE(a.is_equal(b));
}

TEST(EventBatch, EverythingComesFromTheResource) {
  CountingResource counting;
  EventBatch batch = makeEventBatch(100, 100, &counting);
  ASSERT_EQ(batch.size(), 100u);
  // std::pmr::string and std::string are different types (different allocator
  // template argument), so compare through std::string_view.
  EXPECT_EQ(std::string_view(batch[3]), std::string(100, 'd'));
  // 1 for the vector (reserve!) + 1 per string (100 bytes > small-string buffer).
  EXPECT_EQ(counting.allocations(), 101u);
  for (const auto &s : batch) {
    EXPECT_EQ(s.get_allocator().resource(), &counting) << "inner strings must use the batch's resource";
  }
}

TEST(EventBatch, PitfallNonPmrElementsIgnoreTheResource) {
  // A pmr::vector of *std::string*: only the vector's own buffer uses the
  // resource; each std::string still calls global new. Easy to get wrong.
  CountingResource counting;
  std::pmr::vector<std::string> v(&counting);
  v.reserve(10);
  for (int i = 0; i < 10; ++i) {
    v.emplace_back(100, 'x');
  }
  EXPECT_EQ(counting.allocations(), 1u);
}

TEST(Arena, MonotonicBufferBatchesUpstreamAllocations) {
  CountingResource upstream;
  {
    std::pmr::monotonic_buffer_resource arena(&upstream);
    {
      EventBatch batch = makeEventBatch(1000, 100, &arena);
      ASSERT_EQ(batch.size(), 1000u);
      // 1001 allocations from the arena become a handful of big upstream chunks.
      EXPECT_LT(upstream.allocations(), 20u);
    }
    // Deallocation is a no-op in a monotonic arena: memory is still held...
    EXPECT_GT(upstream.bytesOutstanding(), 0u);
  }
  // ...until the arena itself is destroyed.
  EXPECT_EQ(upstream.bytesOutstanding(), 0u);
}

TEST(Arena, StackBufferMeansZeroHeapAllocations) {
  CountingResource upstream;
  alignas(std::max_align_t) std::array<std::byte, 16 * 1024> buffer;
  std::pmr::monotonic_buffer_resource arena(buffer.data(), buffer.size(), &upstream);
  EventBatch batch = makeEventBatch(50, 100, &arena);
  ASSERT_EQ(batch.size(), 50u);
  EXPECT_EQ(upstream.allocations(), 0u) << "everything should fit in the stack buffer";
}

TEST(Bench, DefaultAllocatorVsArena) {
  // Not a real benchmark (use Google Benchmark or perf for that), but enough
  // to see the difference. Build with -DCMAKE_BUILD_TYPE=Release to compare fairly.
  constexpr size_t kN = 200000, kSize = 64;
  auto t0 = std::chrono::steady_clock::now();
  size_t a = makeEventBatchStd(kN, kSize).size();
  auto t1 = std::chrono::steady_clock::now();
  size_t b = 0;
  {
    std::pmr::monotonic_buffer_resource arena;
    b = makeEventBatch(kN, kSize, &arena).size();
  }
  auto t2 = std::chrono::steady_clock::now();
  using us = std::chrono::microseconds;
  std::printf("  std::allocator : %lld us\n  monotonic arena: %lld us\n",
              static_cast<long long>(std::chrono::duration_cast<us>(t1 - t0).count()),
              static_cast<long long>(std::chrono::duration_cast<us>(t2 - t1).count()));
  EXPECT_EQ(a, kN);
  EXPECT_EQ(b, kN);
}
