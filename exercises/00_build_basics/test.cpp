#include <gtest/gtest.h>

#include "str_util.h"

using j2c::StrUtil;

TEST(StrUtil, SplitKeepsEmptyTokens) {
  std::vector<std::string> expected{"a", "b", "", "c"};
  EXPECT_EQ(StrUtil::split("a,b,,c", ','), expected);
}

TEST(StrUtil, SplitWithoutDelimiterReturnsWholeString) {
  std::vector<std::string> expected{"abc"};
  EXPECT_EQ(StrUtil::split("abc", ','), expected);
}

TEST(StrUtil, SplitTrailingDelimiter) {
  std::vector<std::string> expected{"a", ""};
  EXPECT_EQ(StrUtil::split("a,", ','), expected);
}

TEST(StrUtil, Trim) {
  EXPECT_EQ(StrUtil::trim("  hello world \t\n"), "hello world");
  EXPECT_EQ(StrUtil::trim("   "), "");
  EXPECT_EQ(StrUtil::trim(""), "");
  EXPECT_EQ(StrUtil::trim("x"), "x");
}

TEST(StrUtil, Join) {
  EXPECT_EQ(StrUtil::join({"a", "b", "c"}, ", "), "a, b, c");
  EXPECT_EQ(StrUtil::join({}, ","), "");
  EXPECT_EQ(StrUtil::join({"solo"}, ","), "solo");
}

TEST(StrUtil, StartsWith) {
  EXPECT_TRUE(StrUtil::startsWith("gringofts", "grin"));
  EXPECT_TRUE(StrUtil::startsWith("gringofts", ""));
  EXPECT_FALSE(StrUtil::startsWith("grin", "gringofts"));
  EXPECT_FALSE(StrUtil::startsWith("raft", "log"));
}
