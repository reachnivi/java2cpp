#include <gtest/gtest.h>

#include "refs.h"

using namespace j2c;  // NOLINT

TEST(Refs, IncrementModifiesCaller) {
  int x = 41;
  increment(x);
  EXPECT_EQ(x, 42);
}

TEST(Refs, Swap) {
  int a = 1, b = 2;
  swapValues(a, b);
  EXPECT_EQ(a, 2);
  EXPECT_EQ(b, 1);
}

TEST(Refs, SumAll) {
  EXPECT_EQ(sumAll({1, 2, 3, 4}), 10);
  EXPECT_EQ(sumAll({}), 0);
}

TEST(Refs, TryParseOk) {
  int out = -1;
  EXPECT_TRUE(tryParse("123", &out));
  EXPECT_EQ(out, 123);
  EXPECT_TRUE(tryParse("-7", &out));
  EXPECT_EQ(out, -7);
}

TEST(Refs, TryParseRejectsGarbageAndNull) {
  int out = 99;
  EXPECT_FALSE(tryParse("12abc", &out));
  EXPECT_FALSE(tryParse("", &out));
  EXPECT_FALSE(tryParse("abc", &out));
  EXPECT_EQ(out, 99);  // untouched on failure
  EXPECT_FALSE(tryParse("5", nullptr));
}

TEST(Refs, LongestReturnsReferenceIntoVector) {
  std::vector<std::string> v{"raft", "gringofts", "log"};
  const std::string &l = longest(v);
  EXPECT_EQ(l, "gringofts");
  EXPECT_EQ(&l, &v[1]) << "longest() must return a reference, not a copy";
}

TEST(Refs, LongestThrowsOnEmpty) {
  std::vector<std::string> v;
  EXPECT_THROW(longest(v), std::invalid_argument);
}

TEST(Refs, AppendEventViaOutPointer) {
  std::vector<std::string> events;
  appendEvent(&events, "Increased");
  appendEvent(&events, "Increased");
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0], "Increased");
}
