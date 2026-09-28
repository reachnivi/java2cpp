#include <gtest/gtest.h>

#include "smart.h"

using namespace j2c;  // NOLINT

TEST(Smart, MakeWidget) {
  auto w = makeWidget(7);
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->id(), 7);
}

TEST(Smart, ConsumeTakesOwnershipAndDestroys) {
  int before = Widget::alive();
  auto w = makeWidget(3);
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(Widget::alive(), before + 1);
  EXPECT_EQ(consume(std::move(w)), 3);
  EXPECT_EQ(w, nullptr);  // NOLINT: moved-from unique_ptr is guaranteed null
  EXPECT_EQ(Widget::alive(), before);
}

TEST(Smart, TreeParentAndChildren) {
  auto root = TreeNode::create("root");
  auto a = TreeNode::create("a");
  auto b = TreeNode::create("b");
  TreeNode::addChild(root, a);
  TreeNode::addChild(a, b);
  EXPECT_EQ(root->children().size(), 1u);
  ASSERT_NE(b->parent(), nullptr);
  EXPECT_EQ(b->parent()->name(), "a");
  EXPECT_EQ(a->parent(), root);
  EXPECT_EQ(root->parent(), nullptr);
}

TEST(Smart, DroppingRootFreesWholeTreeNoCycleLeak) {
  int before = TreeNode::alive();
  {
    auto root = TreeNode::create("root");
    for (int i = 0; i < 3; ++i) {
      auto child = TreeNode::create("c" + std::to_string(i));
      TreeNode::addChild(root, child);
      TreeNode::addChild(child, TreeNode::create("grandchild"));
    }
    EXPECT_EQ(TreeNode::alive(), before + 7);
  }
  EXPECT_EQ(TreeNode::alive(), before) << "cycle leak: is mParent a shared_ptr?";
}

TEST(Smart, ParentGoneMeansNull) {
  auto child = TreeNode::create("orphan");
  {
    auto parent = TreeNode::create("p");
    TreeNode::addChild(parent, child);
    EXPECT_NE(child->parent(), nullptr);
  }
  // parent still alive? No: only `child` is referenced from here; the parent's
  // last owner (the local) is gone.
  EXPECT_EQ(child->parent(), nullptr);
}

TEST(Smart, RegistryHandsOutNonOwningPointers) {
  Registry r;
  r.add(makeWidget(1));
  r.add(makeWidget(2));
  Widget *w = r.find(2);
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->id(), 2);
  EXPECT_EQ(r.find(99), nullptr);
  EXPECT_EQ(r.size(), 2u);
}
