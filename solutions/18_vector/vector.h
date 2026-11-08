#ifndef J2C_VECTOR_H_
#define J2C_VECTOR_H_

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace j2c {

// A simplified std::vector. Storage is raw memory: objects are created with
// placement new and destroyed with explicit destructor calls, so "capacity"
// and "size" are genuinely different things.
template <typename T>
class Vector {
 public:
  using iterator = T *;
  using const_iterator = const T *;

  Vector() noexcept = default;

  Vector(std::initializer_list<T> init) {
    reserve(init.size());
    for (const T &v : init) {
      push_back(v);
    }
  }

  Vector(const Vector &other) : mData(allocate(other.mSize)), mCapacity(other.mSize) {
    // If a copy throws, destroy what we built and free the memory: no leak.
    size_t i = 0;
    try {
      for (; i < other.mSize; ++i) {
        new (mData + i) T(other.mData[i]);
      }
    } catch (...) {
      destroyRange(mData, mData + i);
      deallocate(mData);
      throw;
    }
    mSize = other.mSize;
  }

  Vector(Vector &&other) noexcept
      : mData(std::exchange(other.mData, nullptr)),
        mSize(std::exchange(other.mSize, 0)),
        mCapacity(std::exchange(other.mCapacity, 0)) {}

  // Copy-and-swap: strong guarantee for free.
  Vector &operator=(const Vector &other) {
    if (this != &other) {
      Vector tmp(other);
      swap(tmp);
    }
    return *this;
  }

  Vector &operator=(Vector &&other) noexcept {
    if (this != &other) {
      Vector tmp(std::move(other));
      swap(tmp);
    }
    return *this;
  }

  ~Vector() {
    destroyRange(mData, mData + mSize);
    deallocate(mData);
  }

  void swap(Vector &other) noexcept {
    std::swap(mData, other.mData);
    std::swap(mSize, other.mSize);
    std::swap(mCapacity, other.mCapacity);
  }

  size_t size() const noexcept { return mSize; }
  size_t capacity() const noexcept { return mCapacity; }
  bool empty() const noexcept { return mSize == 0; }

  T &operator[](size_t i) { return mData[i]; }
  const T &operator[](size_t i) const { return mData[i]; }

  T &at(size_t i) {
    if (i >= mSize) {
      throw std::out_of_range("Vector::at");
    }
    return mData[i];
  }
  const T &at(size_t i) const { return const_cast<Vector *>(this)->at(i); }

  T &back() { return mData[mSize - 1]; }

  iterator begin() noexcept { return mData; }
  iterator end() noexcept { return mData + mSize; }
  const_iterator begin() const noexcept { return mData; }
  const_iterator end() const noexcept { return mData + mSize; }

  // Strong guarantee: if relocation throws, *this is unchanged.
  void reserve(size_t newCapacity) {
    if (newCapacity <= mCapacity) {
      return;
    }
    T *newData = allocate(newCapacity);
    size_t i = 0;
    try {
      for (; i < mSize; ++i) {
        // Moves only if the move can't throw (or T can't be copied);
        // otherwise copies, so a failure leaves the old buffer intact.
        new (newData + i) T(std::move_if_noexcept(mData[i]));
      }
    } catch (...) {
      destroyRange(newData, newData + i);
      deallocate(newData);
      throw;
    }
    destroyRange(mData, mData + mSize);
    deallocate(mData);
    mData = newData;
    mCapacity = newCapacity;
  }

  template <typename... Args>
  T &emplace_back(Args &&...args) {
    if (mSize == mCapacity) {
      // Build the new element in the new buffer first: args may refer to an
      // element of *this (v.push_back(v[0])), which reallocation would free.
      size_t newCapacity = mCapacity == 0 ? 1 : mCapacity * 2;
      T *newData = allocate(newCapacity);
      try {
        new (newData + mSize) T(std::forward<Args>(args)...);
      } catch (...) {
        deallocate(newData);
        throw;
      }
      size_t i = 0;
      try {
        for (; i < mSize; ++i) {
          new (newData + i) T(std::move_if_noexcept(mData[i]));
        }
      } catch (...) {
        destroyRange(newData, newData + i);
        newData[mSize].~T();
        deallocate(newData);
        throw;
      }
      destroyRange(mData, mData + mSize);
      deallocate(mData);
      mData = newData;
      mCapacity = newCapacity;
    } else {
      new (mData + mSize) T(std::forward<Args>(args)...);
    }
    return mData[mSize++];
  }

  void push_back(const T &v) { emplace_back(v); }
  void push_back(T &&v) { emplace_back(std::move(v)); }

  void pop_back() {
    if (mSize == 0) {
      throw std::out_of_range("Vector::pop_back on empty");
    }
    mData[--mSize].~T();
  }

  void resize(size_t n) {
    if (n < mSize) {
      destroyRange(mData + n, mData + mSize);
      mSize = n;
      return;
    }
    reserve(n);
    while (mSize < n) {
      new (mData + mSize) T();
      ++mSize;
    }
  }

  void clear() noexcept {
    destroyRange(mData, mData + mSize);
    mSize = 0;
  }

 private:
  static T *allocate(size_t n) {
    if (n == 0) {
      return nullptr;
    }
    return static_cast<T *>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
  }
  static void deallocate(T *p) noexcept {
    if (p != nullptr) {
      ::operator delete(p, std::align_val_t{alignof(T)});
    }
  }
  static void destroyRange(T *first, T *last) noexcept {
    for (; first != last; ++first) {
      first->~T();
    }
  }

  T *mData = nullptr;
  size_t mSize = 0;
  size_t mCapacity = 0;
};

}  // namespace j2c

#endif  // J2C_VECTOR_H_
