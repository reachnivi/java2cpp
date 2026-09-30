#ifndef J2C_SMART_H_
#define J2C_SMART_H_

#include <memory>
#include <string>
#include <vector>

namespace j2c {

class Widget {
 public:
  explicit Widget(int id) : mId(id) { ++sAlive; }
  ~Widget() { --sAlive; }
  int id() const { return mId; }
  static int alive() { return sAlive; }

 private:
  int mId;
  inline static int sAlive = 0;
};

inline std::unique_ptr<Widget> makeWidget(int id) {
  // TODO: std::make_unique
  (void)id;
  return nullptr;
}

inline int consume(std::unique_ptr<Widget> w) {
  // TODO: return the id. What happens to *w when this function returns?
  (void)w;
  return -1;
}

class TreeNode {
 public:
  static std::shared_ptr<TreeNode> create(std::string name) {
    // TODO: std::make_shared can't call a private ctor, so this uses `new`
    // wrapped straight into a shared_ptr. (Or make the ctor public.)
    return std::shared_ptr<TreeNode>(new TreeNode(std::move(name)));
  }
  ~TreeNode() { --sAlive; }

  static void addChild(const std::shared_ptr<TreeNode> &parent, std::shared_ptr<TreeNode> child) {
    // TODO: set child's parent and push child into parent's children.
    (void)parent;
    (void)child;
  }

  std::shared_ptr<TreeNode> parent() const {
    // TODO: lock the weak_ptr
    return nullptr;
  }

  const std::vector<std::shared_ptr<TreeNode>> &children() const { return mChildren; }
  const std::string &name() const { return mName; }
  static int alive() { return sAlive; }

 private:
  explicit TreeNode(std::string name) : mName(std::move(name)) { ++sAlive; }

  std::string mName;
  std::vector<std::shared_ptr<TreeNode>> mChildren;
  // TODO: add `std::weak_ptr<TreeNode> mParent;`
  inline static int sAlive = 0;
};

class Registry {
 public:
  void add(std::unique_ptr<Widget> w) {
    // TODO
    (void)w;
  }
  Widget *find(int id) const {
    // TODO: return .get() of the matching unique_ptr, or nullptr
    (void)id;
    return nullptr;
  }
  std::size_t size() const { return mWidgets.size(); }

 private:
  std::vector<std::unique_ptr<Widget>> mWidgets;
};

}  // namespace j2c

#endif  // J2C_SMART_H_
