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

inline std::unique_ptr<Widget> makeWidget(int id) { return std::make_unique<Widget>(id); }

// `w` owns the Widget; it's destroyed when `w` goes out of scope at return.
inline int consume(std::unique_ptr<Widget> w) { return w->id(); }

class TreeNode {
 public:
  static std::shared_ptr<TreeNode> create(std::string name) {
    return std::shared_ptr<TreeNode>(new TreeNode(std::move(name)));
  }
  ~TreeNode() { --sAlive; }

  static void addChild(const std::shared_ptr<TreeNode> &parent, std::shared_ptr<TreeNode> child) {
    child->mParent = parent;
    parent->mChildren.push_back(std::move(child));
  }

  std::shared_ptr<TreeNode> parent() const { return mParent.lock(); }

  const std::vector<std::shared_ptr<TreeNode>> &children() const { return mChildren; }
  const std::string &name() const { return mName; }
  static int alive() { return sAlive; }

 private:
  explicit TreeNode(std::string name) : mName(std::move(name)) { ++sAlive; }

  std::string mName;
  std::vector<std::shared_ptr<TreeNode>> mChildren;
  std::weak_ptr<TreeNode> mParent;
  inline static int sAlive = 0;
};

class Registry {
 public:
  void add(std::unique_ptr<Widget> w) { mWidgets.push_back(std::move(w)); }
  Widget *find(int id) const {
    for (const auto &w : mWidgets) {
      if (w->id() == id) {
        return w.get();
      }
    }
    return nullptr;
  }
  std::size_t size() const { return mWidgets.size(); }

 private:
  std::vector<std::unique_ptr<Widget>> mWidgets;
};

}  // namespace j2c

#endif  // J2C_SMART_H_
