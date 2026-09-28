#include <gtest/gtest.h>

#include <list>
#include <set>
#include <string>
#include <vector>

#include "templates.h"

using namespace j2c;  // NOLINT

TEST(Templates, RingBufferFifo) {
  RingBuffer<int, 3> rb;
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ((RingBuffer<int, 3>::capacity()), 3u);
  EXPECT_TRUE(rb.push(1));
  EXPECT_TRUE(rb.push(2));
  EXPECT_TRUE(rb.push(3));
  EXPECT_TRUE(rb.full());
  EXPECT_FALSE(rb.push(4));
  EXPECT_EQ(rb.pop(), 1);
  EXPECT_TRUE(rb.push(4));  // wraps around
  EXPECT_EQ(rb.pop(), 2);
  EXPECT_EQ(rb.pop(), 3);
  EXPECT_EQ(rb.pop(), 4);
  EXPECT_EQ(rb.pop(), std::nullopt);
  EXPECT_EQ(rb.size(), 0u);
}

TEST(Templates, RingBufferWorksWithNonTrivialTypes) {
  RingBuffer<std::string, 2> rb;
  EXPECT_TRUE(rb.push("raft"));
  EXPECT_TRUE(rb.push("log"));
  EXPECT_EQ(rb.pop().value_or(""), "raft");
  EXPECT_EQ(rb.size(), 1u);
}

TEST(Templates, CountIfAnyContainerAnyCallable) {
  std::vector<int> v{1, 2, 3, 4, 5, 6};
  EXPECT_EQ(countIf(v, [](int x) { return x % 2 == 0; }), 3u);

  std::list<std::string> l{"a", "bb", "ccc"};
  EXPECT_EQ(countIf(l, [](const std::string &s) { return s.size() > 1; }), 2u);

  std::set<int> s{10, 20, 30};
  struct GreaterThan15 {
    bool operator()(int x) const { return x > 15; }
  };
  EXPECT_EQ(countIf(s, GreaterThan15{}), 2u);
}

TEST(Templates, DescribeWithIfConstexpr) {
  EXPECT_EQ(describe(42), "int:42");
  EXPECT_EQ(describe(static_cast<uint64_t>(7)), "int:7");
  EXPECT_EQ(describe(3.14), "float");
  EXPECT_EQ(describe(std::string("hi")), "string:hi");
  EXPECT_EQ(describe(std::vector<int>{}), "other");
}

TEST(Templates, VariadicMax) {
  EXPECT_EQ(maxOf(1, 2), 2);
  EXPECT_EQ(maxOf(5, 1, 9, 3), 9);
  EXPECT_EQ(maxOf(std::string("a"), std::string("c"), std::string("b")), "c");
}
