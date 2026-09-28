#ifndef J2C_BUFFER_H_
#define J2C_BUFFER_H_

#include <cstddef>
#include <vector>

namespace j2c {

class Buffer {
 public:
  explicit Buffer(std::size_t size) {
    // TODO: allocate `new char[size]()` (the () zero-fills), record size,
    // and ++sAllocations.
    (void)size;
  }

  ~Buffer() {
    // TODO: delete[] mData;
  }

  // TODO: copy constructor (deep copy)
  // TODO: copy assignment (deep copy; handle self-assignment)
  // TODO: move constructor (noexcept; steal; leave other empty)
  // TODO: move assignment (noexcept; free own data first)

  char *data() { return mData; }
  const char *data() const { return mData; }
  std::size_t size() const { return mSize; }

  static int allocations() { return sAllocations; }
  static void resetAllocations() { sAllocations = 0; }

 private:
  char *mData = nullptr;
  std::size_t mSize = 0;
  inline static int sAllocations = 0;
};

// Rule of 0: let std::vector do the work. No destructor, no copy/move members.
class BufferRule0 {
 public:
  explicit BufferRule0(std::size_t size) {
    // TODO: size the vector
    (void)size;
  }
  char *data() { return mData.data(); }
  std::size_t size() const { return mData.size(); }

 private:
  std::vector<char> mData;
};

}  // namespace j2c

#endif  // J2C_BUFFER_H_
