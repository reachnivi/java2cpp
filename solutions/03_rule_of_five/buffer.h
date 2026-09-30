#ifndef J2C_BUFFER_H_
#define J2C_BUFFER_H_

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace j2c {

class Buffer {
 public:
  explicit Buffer(std::size_t size) : mData(new char[size]()), mSize(size) { ++sAllocations; }

  ~Buffer() { delete[] mData; }

  Buffer(const Buffer &other) : mData(new char[other.mSize]), mSize(other.mSize) {
    ++sAllocations;
    std::copy(other.mData, other.mData + other.mSize, mData);
  }

  // Copy-and-swap: build the copy first, then swap guts with it. If the
  // allocation throws, *this is untouched (strong exception guarantee).
  Buffer &operator=(const Buffer &other) {
    if (this != &other) {
      Buffer tmp(other);
      swap(tmp);
    }
    return *this;
  }

  Buffer(Buffer &&other) noexcept
      : mData(std::exchange(other.mData, nullptr)), mSize(std::exchange(other.mSize, 0)) {}

  Buffer &operator=(Buffer &&other) noexcept {
    if (this != &other) {
      delete[] mData;
      mData = std::exchange(other.mData, nullptr);
      mSize = std::exchange(other.mSize, 0);
    }
    return *this;
  }

  void swap(Buffer &other) noexcept {
    std::swap(mData, other.mData);
    std::swap(mSize, other.mSize);
  }

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

class BufferRule0 {
 public:
  explicit BufferRule0(std::size_t size) : mData(size) {}
  char *data() { return mData.data(); }
  std::size_t size() const { return mData.size(); }

 private:
  std::vector<char> mData;
};

}  // namespace j2c

#endif  // J2C_BUFFER_H_
