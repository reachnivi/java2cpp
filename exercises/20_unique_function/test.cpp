#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "unique_function.h"

using j2c::UniqueFunction;

namespace {

struct Counted {
  static inline int alive = 0;
  Counted() { ++alive; }
  Counted(const Counted &) { ++alive; }
  Counted(Counted &&) noexcept { ++alive; }
  ~Counted() { --alive; }
};

int addOne(int x) { return x + 1; }

}  // namespace

TEST(UniqueFunctionTest, IsMoveOnly) {
  EXPECT_FALSE((std::is_copy_constructible_v<UniqueFunction<void()>>));
  EXPECT_TRUE((std::is_nothrow_move_constructible_v<UniqueFunction<void()>>));
}

TEST(UniqueFunctionTest, CallsLambdasFunctionPointersAndFunctors) {
  UniqueFunction<int(int)> f = [](int x) { return x * 2; };
  ASSERT_TRUE(f);
  EXPECT_EQ(f(21), 42);
  UniqueFunction<int(int)> g = addOne;
  EXPECT_EQ(g(1), 2);
  UniqueFunction<int(int)> h = std::negate<int>();
  EXPECT_EQ(h(5), -5);
}

TEST(UniqueFunctionTest, HoldsMoveOnlyCaptures) {
  // std::function cannot do this: it requires a copyable callable.
  auto p = std::make_unique<std::string>("raft");
  UniqueFunction<size_t()> f = [q = std::move(p)] { return q->size(); };
  EXPECT_EQ(f(), 4u);
  // Try it: `std::function<size_t()> g = [q = std::make_unique<int>()] { return size_t{0}; };`
  // does not compile ("use of deleted function ... unique_ptr(const unique_ptr&)").
}

TEST(UniqueFunctionTest, SmallCallablesAreInline) {
  int base = 10;
  UniqueFunction<int(int)> small = [base](int x) { return base + x; };
  EXPECT_TRUE(small.isInline()) << "an int capture must not allocate";
  EXPECT_EQ(small(5), 15);

  std::array<char, 256> big{};
  big[0] = 7;
  UniqueFunction<int()> large = [big] { return static_cast<int>(big[0]); };
  EXPECT_FALSE(large.isInline()) << "256 bytes can't fit in the small buffer";
  EXPECT_EQ(large(), 7);
}

TEST(UniqueFunctionTest, MoveKeepsWorkingInBothModes) {
  int base = 1;
  UniqueFunction<int(int)> a = [base](int x) { return base + x; };
  UniqueFunction<int(int)> b = std::move(a);
  EXPECT_FALSE(a);  // NOLINT: moved-from must be empty
  ASSERT_TRUE(b);
  EXPECT_EQ(b(1), 2);

  std::array<int, 64> big{};
  big[3] = 4;
  UniqueFunction<int()> c = [big] { return big[3]; };
  UniqueFunction<int()> d;
  d = std::move(c);
  EXPECT_FALSE(c);  // NOLINT
  EXPECT_EQ(d(), 4);
}

TEST(UniqueFunctionTest, DestroysCapturesExactlyOnce) {
  Counted::alive = 0;
  {
    Counted c;
    UniqueFunction<void()> f = [c] {};
    UniqueFunction<void()> g = std::move(f);
    UniqueFunction<void()> h;
    h = std::move(g);
    EXPECT_EQ(Counted::alive, 2) << "the local + exactly one capture";
  }
  EXPECT_EQ(Counted::alive, 0);
  {
    std::array<Counted, 50> many;
    UniqueFunction<void()> f = [many] {};  // heap mode
    UniqueFunction<void()> g = std::move(f);
    EXPECT_EQ(Counted::alive, 100);
  }
  EXPECT_EQ(Counted::alive, 0);
}

TEST(UniqueFunctionTest, ReassignDestroysOldCallable) {
  Counted::alive = 0;
  UniqueFunction<void()> f = [c = Counted()] {};
  EXPECT_EQ(Counted::alive, 1);
  f = [] {};
  EXPECT_EQ(Counted::alive, 0);
  f = nullptr;
  EXPECT_FALSE(f);
}

TEST(UniqueFunctionTest, EmptyCallThrows) {
  UniqueFunction<void()> f;
  EXPECT_FALSE(f);
  EXPECT_THROW(f(), std::bad_function_call);
}

TEST(UniqueFunctionTest, StatefulCallableKeepsState) {
  UniqueFunction<int()> counter = [n = 0]() mutable { return ++n; };
  counter();
  counter();
  EXPECT_EQ(counter(), 3);
}

TEST(UniqueFunctionTest, WorksInContainers) {
  std::vector<UniqueFunction<int()>> tasks;
  for (int i = 0; i < 100; ++i) {
    tasks.emplace_back([p = std::make_unique<int>(i)] { return *p; });
  }
  int sum = 0;
  for (auto &t : tasks) {
    sum += t();
  }
  EXPECT_EQ(sum, 4950);
}
