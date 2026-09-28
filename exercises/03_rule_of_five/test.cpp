#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "buffer.h"

using j2c::Buffer;
using j2c::BufferRule0;

class BufferTest : public ::testing::Test {
 protected:
  void SetUp() override { Buffer::resetAllocations(); }
};

TEST_F(BufferTest, ConstructAllocatesZeroFilled) {
  Buffer b(4);
  EXPECT_EQ(b.size(), 4u);
  ASSERT_NE(b.data(), nullptr);
  for (std::size_t i = 0; i < b.size(); ++i) {
    EXPECT_EQ(b.data()[i], 0);
  }
  EXPECT_EQ(Buffer::allocations(), 1);
}

TEST_F(BufferTest, CopyIsDeep) {
  Buffer a(3);
  ASSERT_NE(a.data(), nullptr);
  a.data()[0] = 'x';
  Buffer b(a);
  EXPECT_EQ(Buffer::allocations(), 2);
  EXPECT_NE(a.data(), b.data());
  EXPECT_EQ(b.data()[0], 'x');
  b.data()[0] = 'y';
  EXPECT_EQ(a.data()[0], 'x');
}

TEST_F(BufferTest, CopyAssignIsDeepAndSelfSafe) {
  Buffer a(3);
  ASSERT_NE(a.data(), nullptr);
  a.data()[1] = 'q';
  Buffer b(1);
  b = a;
  EXPECT_EQ(b.size(), 3u);
  EXPECT_EQ(b.data()[1], 'q');
  EXPECT_NE(a.data(), b.data());
  b = b;  // NOLINT: self-assignment on purpose
  EXPECT_EQ(b.size(), 3u);
  EXPECT_EQ(b.data()[1], 'q');
}

TEST_F(BufferTest, MoveStealsWithoutAllocating) {
  Buffer a(8);
  char *original = a.data();
  Buffer b(std::move(a));
  EXPECT_EQ(Buffer::allocations(), 1);
  EXPECT_EQ(b.data(), original);
  EXPECT_EQ(b.size(), 8u);
  EXPECT_EQ(a.data(), nullptr);  // NOLINT: inspecting moved-from on purpose
  EXPECT_EQ(a.size(), 0u);
}

TEST_F(BufferTest, MoveAssign) {
  Buffer a(8);
  Buffer b(2);
  char *original = a.data();
  b = std::move(a);
  EXPECT_EQ(b.data(), original);
  EXPECT_EQ(b.size(), 8u);
  EXPECT_EQ(a.data(), nullptr);  // NOLINT
  EXPECT_EQ(Buffer::allocations(), 2);
}

TEST_F(BufferTest, MoveIsNoexceptSoVectorUsesIt) {
  EXPECT_TRUE(std::is_nothrow_move_constructible_v<Buffer>);
  EXPECT_TRUE(std::is_nothrow_move_assignable_v<Buffer>);
  std::vector<Buffer> v;
  for (int i = 0; i < 20; ++i) {
    v.emplace_back(16);  // constructs in place: exactly one allocation each
  }
  // If growth copied instead of moved, we'd see far more than 20.
  EXPECT_EQ(Buffer::allocations(), 20);
}

TEST(BufferRule0Test, SameSemanticsForFree) {
  BufferRule0 a(3);
  ASSERT_EQ(a.size(), 3u);
  a.data()[0] = 'x';
  BufferRule0 b = a;
  b.data()[0] = 'y';
  EXPECT_EQ(a.data()[0], 'x');
  BufferRule0 c = std::move(a);
  EXPECT_EQ(c.size(), 3u);
  EXPECT_TRUE(std::is_nothrow_move_constructible_v<BufferRule0>);
}
