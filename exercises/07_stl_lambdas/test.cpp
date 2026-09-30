#include <gtest/gtest.h>

#include "stl.h"

using namespace j2c;  // NOLINT

static std::vector<Order> sample() {
  return {
      {1, "alice", 30.0}, {2, "bob", 5.0},  {3, "alice", 12.5},
      {4, "carol", 99.0}, {5, "bob", 50.0}, {6, "dave", 1.0},
  };
}

TEST(Stl, TotalByUser) {
  auto totals = totalByUser(sample());
  ASSERT_EQ(totals.size(), 4u);
  EXPECT_DOUBLE_EQ(totals["alice"], 42.5);
  EXPECT_DOUBLE_EQ(totals["bob"], 55.0);
  EXPECT_EQ(totals.begin()->first, "alice");  // std::map is ordered
}

TEST(Stl, IdsAboveKeepsOrder) {
  std::vector<int> expected{1, 4, 5};
  EXPECT_EQ(idsAbove(sample(), 20.0), expected);
  EXPECT_TRUE(idsAbove(sample(), 1000.0).empty());
}

TEST(Stl, TopN) {
  auto top = topN(sample(), 2);
  ASSERT_EQ(top.size(), 2u);
  EXPECT_EQ(top[0].id, 4);
  EXPECT_EQ(top[1].id, 5);
  EXPECT_EQ(topN(sample(), 100).size(), 6u);
}

TEST(Stl, RemoveSmallInPlace) {
  auto orders = sample();
  removeSmall(&orders, 10.0);
  ASSERT_EQ(orders.size(), 4u);
  for (const auto &o : orders) {
    EXPECT_GE(o.amount, 10.0);
  }
  EXPECT_EQ(orders[0].id, 1);  // relative order preserved
}

TEST(Stl, DistinctUsersSorted) {
  std::vector<std::string> expected{"alice", "bob", "carol", "dave"};
  EXPECT_EQ(distinctUsersSorted(sample()), expected);
}

TEST(Stl, IndependentCounters) {
  auto c1 = makeCounter();
  auto c2 = makeCounter();
  ASSERT_TRUE(static_cast<bool>(c1));
  ASSERT_TRUE(static_cast<bool>(c2));
  EXPECT_EQ(c1(), 1);
  EXPECT_EQ(c1(), 2);
  EXPECT_EQ(c2(), 1);
  EXPECT_EQ(c1(), 3);
}

TEST(Stl, CountWords) {
  auto counts = countWords("raft log raft  state\nmachine log raft");
  EXPECT_EQ(counts["raft"], 3);
  EXPECT_EQ(counts["log"], 2);
  EXPECT_EQ(counts["machine"], 1);
  EXPECT_EQ(counts.size(), 4u);
}
