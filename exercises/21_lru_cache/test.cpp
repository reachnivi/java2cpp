#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "lru_cache.h"

using j2c::ConcurrentLru;
using j2c::LruCache;

namespace {
std::vector<int> order(const LruCache<int, std::string> &c) {
  std::vector<int> keys;
  c.keysByRecency(std::back_inserter(keys));
  return keys;
}
}  // namespace

TEST(LruCacheTest, ZeroCapacityRejected) { EXPECT_THROW((LruCache<int, int>(0)), std::invalid_argument); }

TEST(LruCacheTest, GetAndPut) {
  LruCache<int, std::string> c(2);
  EXPECT_EQ(c.get(1), std::nullopt);
  c.put(1, "one");
  c.put(2, "two");
  EXPECT_EQ(c.get(1), "one");
  EXPECT_EQ(c.get(2), "two");
  EXPECT_EQ(c.size(), 2u);
}

TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
  LruCache<int, std::string> c(2);
  c.put(1, "one");
  c.put(2, "two");
  EXPECT_EQ(c.put(3, "three"), 1) << "1 is least recently used";
  EXPECT_FALSE(c.contains(1));
  EXPECT_TRUE(c.contains(2));
  EXPECT_TRUE(c.contains(3));
}

TEST(LruCacheTest, GetRefreshesRecency) {
  LruCache<int, std::string> c(2);
  c.put(1, "one");
  c.put(2, "two");
  EXPECT_EQ(c.get(1), "one");  // 1 is now most recent
  EXPECT_EQ(c.put(3, "three"), 2);
  EXPECT_TRUE(c.contains(1));
  EXPECT_FALSE(c.contains(2));
}

TEST(LruCacheTest, UpdateExistingKeyRefreshesAndDoesNotGrow) {
  LruCache<int, std::string> c(2);
  c.put(1, "one");
  c.put(2, "two");
  EXPECT_EQ(c.put(1, "uno"), std::nullopt);
  EXPECT_EQ(c.size(), 2u);
  EXPECT_EQ(c.get(1), "uno");
  EXPECT_EQ(c.put(3, "three"), 2);
}

TEST(LruCacheTest, RecencyOrderIsExact) {
  LruCache<int, std::string> c(4);
  for (int k : {1, 2, 3, 4}) {
    c.put(k, std::to_string(k));
  }
  c.get(2);
  c.get(1);
  c.put(3, "3b");
  std::vector<int> expected{3, 1, 2, 4};
  EXPECT_EQ(order(c), expected);
}

TEST(LruCacheTest, CapacityOne) {
  LruCache<int, std::string> c(1);
  c.put(1, "one");
  EXPECT_EQ(c.put(2, "two"), 1);
  EXPECT_EQ(c.get(1), std::nullopt);
  EXPECT_EQ(c.get(2), "two");
  EXPECT_EQ(c.size(), 1u);
}

TEST(LruCacheTest, Erase) {
  LruCache<int, std::string> c(3);
  c.put(1, "one");
  c.put(2, "two");
  EXPECT_TRUE(c.erase(1));
  EXPECT_FALSE(c.erase(1));
  EXPECT_EQ(c.size(), 1u);
  c.put(3, "three");
  c.put(4, "four");
  EXPECT_EQ(c.size(), 3u) << "erase must free a slot";
  std::vector<int> expected{4, 3, 2};
  EXPECT_EQ(order(c), expected);
}

TEST(LruCacheTest, ManyOperationsStayConsistent) {
  LruCache<int, int> c(100);
  for (int i = 0; i < 100000; ++i) {
    c.put(i % 300, i);
    if (i % 7 == 0) {
      c.get(i % 150);
    }
    if (i % 13 == 0) {
      c.erase(i % 200);
    }
    ASSERT_LE(c.size(), 100u);
  }
}

TEST(ConcurrentLruTest, ParallelGetPutIsSafe) {
  // Run this under the ThreadSanitizer build too.
  ConcurrentLru<int, int> c(64);
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&c, t] {
      for (int i = 0; i < 20000; ++i) {
        int key = (i * 31 + t) % 128;
        if (auto v = c.get(key)) {
          if (*v != key * 10) {
            ADD_FAILURE() << "wrong value for " << key;
            return;
          }
        } else {
          c.put(key, key * 10);
        }
      }
    });
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_LE(c.size(), 64u);
  EXPECT_EQ(c.hits() + c.misses(), 8u * 20000u);
  EXPECT_GT(c.hits(), 0u);
}
