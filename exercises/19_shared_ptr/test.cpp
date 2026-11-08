#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "shared_ptr.h"

using j2c::makeShared;
using j2c::SharedPtr;
using j2c::WeakPtr;

namespace {

struct Node {
  static inline std::atomic<int> alive{0};
  explicit Node(int v = 0) : value(v) { ++alive; }
  ~Node() { --alive; }
  int value;
};

class SharedPtrTest : public ::testing::Test {
 protected:
  void SetUp() override { Node::alive = 0; }
};

}  // namespace

TEST_F(SharedPtrTest, OwnsAndDestroysExactlyOnce) {
  {
    SharedPtr<Node> a(new Node(7));
    ASSERT_TRUE(a);
    EXPECT_EQ(a->value, 7);
    EXPECT_EQ(a.use_count(), 1);
    {
      SharedPtr<Node> b = a;
      SharedPtr<Node> c;
      c = b;
      EXPECT_EQ(a.use_count(), 3);
      EXPECT_EQ(Node::alive, 1);
    }
    EXPECT_EQ(a.use_count(), 1);
  }
  EXPECT_EQ(Node::alive, 0);
}

TEST_F(SharedPtrTest, MoveTransfersWithoutCounting) {
  SharedPtr<Node> a(new Node(1));
  SharedPtr<Node> b = std::move(a);
  EXPECT_FALSE(a);  // NOLINT: moved-from is guaranteed empty
  EXPECT_EQ(b.use_count(), 1);
  SharedPtr<Node> c;
  c = std::move(b);
  EXPECT_EQ(c.use_count(), 1);
  EXPECT_EQ(Node::alive, 1);
}

TEST_F(SharedPtrTest, SelfAssignAndReset) {
  SharedPtr<Node> a(new Node(1));
  a = a;  // NOLINT: self-assignment on purpose
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_EQ(Node::alive, 1);
  a.reset();
  EXPECT_FALSE(a);
  EXPECT_EQ(a.use_count(), 0);
  EXPECT_EQ(Node::alive, 0);
}

TEST_F(SharedPtrTest, CustomDeleterRuns) {
  int deleted = 0;
  {
    Node local(5);
    SharedPtr<Node> p(&local, [&deleted](Node *) { ++deleted; });  // must NOT call delete on a stack object
    SharedPtr<Node> q = p;
  }
  EXPECT_EQ(deleted, 1);
}

TEST_F(SharedPtrTest, MakeSharedIsOneAllocation) {
  auto p = makeShared<Node>(3);
  ASSERT_TRUE(p);
  EXPECT_EQ(p->value, 3);
  // The object must live inside the control block's own allocation.
  auto block = reinterpret_cast<std::uintptr_t>(p.controlBlockAddress());
  auto obj = reinterpret_cast<std::uintptr_t>(p.get());
  ASSERT_NE(block, 0u);
  EXPECT_GT(obj, block);
  EXPECT_LT(obj - block, 256u) << "makeShared must construct the object inside the control block";
}

TEST_F(SharedPtrTest, WeakDoesNotKeepObjectAlive) {
  WeakPtr<Node> w;
  {
    auto p = makeShared<Node>(9);
    w = p;
    EXPECT_FALSE(w.expired());
    auto locked = w.lock();
    ASSERT_TRUE(locked);
    EXPECT_EQ(locked->value, 9);
    EXPECT_EQ(p.use_count(), 2);
  }
  EXPECT_EQ(Node::alive, 0) << "object dies with the last SharedPtr, even though a WeakPtr remains";
  EXPECT_TRUE(w.expired());
  EXPECT_FALSE(w.lock());
}

TEST_F(SharedPtrTest, WeakOutlivingStrongIsSafe) {
  // Control block must survive until the last WeakPtr goes, then be freed.
  // (Run under ASan: a premature free is a heap-use-after-free here.)
  std::vector<WeakPtr<std::string>> weaks;
  {
    auto s = makeShared<std::string>(100, 'x');
    for (int i = 0; i < 10; ++i) {
      weaks.emplace_back(s);
    }
  }
  for (auto &w : weaks) {
    EXPECT_TRUE(w.expired());
  }
  weaks.clear();
}

TEST_F(SharedPtrTest, CountsAreThreadSafe) {
  auto p = makeShared<Node>(1);
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([p] {  // copy in
      for (int i = 0; i < 10000; ++i) {
        SharedPtr<Node> copy = p;
        WeakPtr<Node> weak = copy;
        auto again = weak.lock();
        (void)again;
      }
    });
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(p.use_count(), 1);
  p.reset();
  EXPECT_EQ(Node::alive, 0);
}

TEST_F(SharedPtrTest, LastOwnerOnAnotherThreadDestroys) {
  auto p = makeShared<Node>(1);
  std::thread t([q = std::move(p)]() mutable { q.reset(); });
  t.join();
  EXPECT_EQ(Node::alive, 0);
}
