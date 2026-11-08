#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "vector.h"

using j2c::Vector;

namespace {

// Counts every construction and destruction so leaks and double-destroys show up.
struct Tracked {
  static inline int alive = 0;
  static inline int copies = 0;
  static inline int moves = 0;
  static void reset() { alive = copies = moves = 0; }

  int value;
  explicit Tracked(int v = 0) : value(v) { ++alive; }
  Tracked(const Tracked &o) : value(o.value) {
    ++alive;
    ++copies;
  }
  Tracked(Tracked &&o) noexcept : value(o.value) {
    ++alive;
    ++moves;
  }
  Tracked &operator=(const Tracked &) = default;
  ~Tracked() { --alive; }
};

// Copy throws on a chosen copy; move is NOT noexcept, so Vector must copy on
// reallocation to keep the strong guarantee.
struct ThrowOnCopy {
  static inline int copiesUntilThrow = -1;
  static inline int alive = 0;
  int value;
  explicit ThrowOnCopy(int v) : value(v) { ++alive; }
  ThrowOnCopy(const ThrowOnCopy &o) : value(o.value) {
    if (copiesUntilThrow == 0) {
      throw std::runtime_error("copy failed");
    }
    if (copiesUntilThrow > 0) {
      --copiesUntilThrow;
    }
    ++alive;
  }
  ThrowOnCopy(ThrowOnCopy &&o) : value(o.value) { ++alive; }  // may throw (not noexcept)
  ~ThrowOnCopy() { --alive; }
};

struct NoDefault {
  explicit NoDefault(int v) : value(v) {}
  int value;
};

struct alignas(64) Overaligned {
  int value = 0;
};

class VectorTest : public ::testing::Test {
 protected:
  void SetUp() override { Tracked::reset(); }
};

}  // namespace

TEST_F(VectorTest, StartsEmpty) {
  Vector<int> v;
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.size(), 0u);
  EXPECT_EQ(v.capacity(), 0u);
  EXPECT_EQ(v.begin(), v.end());
}

TEST_F(VectorTest, PushBackAndIndex) {
  Vector<std::string> v;
  for (int i = 0; i < 100; ++i) {
    v.push_back("item-" + std::to_string(i));
  }
  ASSERT_EQ(v.size(), 100u);
  EXPECT_GE(v.capacity(), 100u);
  EXPECT_EQ(v[0], "item-0");
  EXPECT_EQ(v[99], "item-99");
  EXPECT_EQ(v.at(42), "item-42");
  EXPECT_THROW(v.at(100), std::out_of_range);
}

TEST_F(VectorTest, GrowthIsGeometric) {
  Vector<int> v;
  size_t reallocations = 0;
  size_t lastCapacity = v.capacity();
  for (int i = 0; i < 1000; ++i) {
    v.push_back(i);
    if (v.capacity() != lastCapacity) {
      ++reallocations;
      lastCapacity = v.capacity();
    }
  }
  ASSERT_EQ(v.size(), 1000u);
  EXPECT_LE(reallocations, 12u) << "growth must be geometric (amortised O(1) push_back)";
}

TEST_F(VectorTest, EmplaceBackConstructsInPlace) {
  {
    Vector<Tracked> v;
    v.reserve(4);
    v.emplace_back(1);
    v.emplace_back(2);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[1].value, 2);
    EXPECT_EQ(Tracked::copies, 0);
    EXPECT_EQ(Tracked::moves, 0);
    EXPECT_EQ(Tracked::alive, 2);
  }
  EXPECT_EQ(Tracked::alive, 0) << "destructor must destroy every element";
}

TEST_F(VectorTest, ReallocationMovesNoexceptTypes) {
  Vector<Tracked> v;
  for (int i = 0; i < 64; ++i) {
    v.emplace_back(i);
  }
  EXPECT_EQ(Tracked::copies, 0) << "Tracked's move is noexcept: reallocation must move, not copy";
  EXPECT_GT(Tracked::moves, 0);
  EXPECT_EQ(Tracked::alive, 64);
}

TEST_F(VectorTest, NoLeaksThroughEveryOperation) {
  {
    Vector<Tracked> a;
    for (int i = 0; i < 10; ++i) {
      a.emplace_back(i);
    }
    Vector<Tracked> b = a;   // copy
    Vector<Tracked> c = std::move(a);  // move
    b.pop_back();
    b.resize(20);
    b.resize(3);
    c = b;  // copy-assign
    b.clear();
    EXPECT_EQ(Tracked::alive, 3) << "only c's 3 elements remain (a was moved from, b was cleared)";
  }
  EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(VectorTest, CopyIsDeepMoveSteals) {
  Vector<int> a{1, 2, 3};
  Vector<int> b = a;
  ASSERT_EQ(b.size(), 3u);
  b[0] = 99;
  EXPECT_EQ(a[0], 1);
  const int *storage = a.begin();
  Vector<int> c = std::move(a);
  EXPECT_EQ(c.begin(), storage) << "move must steal the buffer";
  EXPECT_EQ(a.size(), 0u);  // NOLINT: checking moved-from state on purpose
  EXPECT_TRUE(std::is_nothrow_move_constructible_v<Vector<int>>);
}

TEST_F(VectorTest, SelfAssignmentIsSafe) {
  Vector<std::string> v{"a", "b"};
  v = v;  // NOLINT: self-assignment on purpose
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[1], "b");
}

TEST_F(VectorTest, PushBackOfOwnElementSurvivesReallocation) {
  Vector<std::string> v;
  v.push_back(std::string(100, 'x'));
  ASSERT_EQ(v.size(), v.capacity()) << "next push_back must reallocate for this test to mean anything";
  v.push_back(v[0]);  // the argument lives in the buffer being replaced
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[1], std::string(100, 'x'));
}

TEST_F(VectorTest, StrongGuaranteeWhenRelocationThrows) {
  ThrowOnCopy::alive = 0;
  ThrowOnCopy::copiesUntilThrow = -1;
  {
    Vector<ThrowOnCopy> v;
    v.reserve(3);
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    const ThrowOnCopy *before = v.begin();
    ThrowOnCopy::copiesUntilThrow = 1;  // second copy during reallocation throws
    EXPECT_THROW(v.reserve(10), std::runtime_error);
    ThrowOnCopy::copiesUntilThrow = -1;
    // Unchanged: same buffer, same size, same values.
    EXPECT_EQ(v.begin(), before);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v[2].value, 3);
    EXPECT_EQ(ThrowOnCopy::alive, 3) << "partially relocated elements must be destroyed";
  }
  EXPECT_EQ(ThrowOnCopy::alive, 0);
}

TEST_F(VectorTest, ElementTypeNeedsNoDefaultConstructor) {
  Vector<NoDefault> v;
  for (int i = 0; i < 10; ++i) {
    v.emplace_back(i);  // reserve/grow must not default-construct anything
  }
  ASSERT_EQ(v.size(), 10u);
  EXPECT_EQ(v[9].value, 9);
}

TEST_F(VectorTest, RespectsOverAlignedTypes) {
  Vector<Overaligned> v;
  for (int i = 0; i < 5; ++i) {
    v.emplace_back();
  }
  ASSERT_EQ(v.size(), 5u);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(v.begin()) % 64, 0u);
}

TEST_F(VectorTest, MoveOnlyElements) {
  Vector<std::unique_ptr<int>> v;
  for (int i = 0; i < 20; ++i) {
    v.push_back(std::make_unique<int>(i));
  }
  ASSERT_EQ(v.size(), 20u);
  EXPECT_EQ(*v[19], 19);
}

TEST_F(VectorTest, PopBackOnEmptyThrows) {
  Vector<int> v;
  EXPECT_THROW(v.pop_back(), std::out_of_range);
}

TEST_F(VectorTest, RangeForWorks) {
  Vector<int> v{1, 2, 3, 4};
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  EXPECT_EQ(sum, 10);
}
