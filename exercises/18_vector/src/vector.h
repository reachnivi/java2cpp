#ifndef J2C_VECTOR_H_
#define J2C_VECTOR_H_

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace j2c {

// A simplified std::vector. Storage is RAW memory: objects are created with
// placement new and destroyed with explicit destructor calls, so "capacity"
// (memory you own) and "size" (objects that exist) are different things.
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

  Vector(const Vector &other) {
    // TODO: allocate other.mSize slots, copy-construct each element with
    // placement new. If a copy throws: destroy what you built, free, rethrow.
    (void)other;
    throw std::logic_error("TODO: copy constructor");
  }

  Vector(Vector &&other) noexcept {
    // TODO: steal other's three members (std::exchange), leave it empty.
    (void)other;
  }

  Vector &operator=(const Vector &other) {
    // TODO: copy-and-swap (see ex 03)
    (void)other;
    throw std::logic_error("TODO: copy assignment");
  }

  Vector &operator=(Vector &&other) noexcept {
    // TODO
    (void)other;
    return *this;
  }

  ~Vector() {
    // TODO: destroy the mSize live elements, then free the raw memory.
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
    // TODO: bounds-check, throw std::out_of_range
    (void)i;
    throw std::logic_error("TODO: at");
  }
  const T &at(size_t i) const { return const_cast<Vector *>(this)->at(i); }

  T &back() { return mData[mSize - 1]; }

  iterator begin() noexcept { return mData; }
  iterator end() noexcept { return mData + mSize; }
  const_iterator begin() const noexcept { return mData; }
  const_iterator end() const noexcept { return mData + mSize; }

  void reserve(size_t newCapacity) {
    // TODO: if growing: allocate a new buffer, relocate each element with
    //   new (newData + i) T(std::move_if_noexcept(mData[i]));
    // (moves if the move can't throw, else copies), then destroy the old
    // elements and free the old buffer. STRONG GUARANTEE: if relocating
    // throws, destroy the partially built new elements, free the new buffer,
    // rethrow, and leave *this untouched.
    (void)newCapacity;
    throw std::logic_error("TODO: reserve");
  }

  template <typename... Args>
  T &emplace_back(Args &&...args) {
    // TODO: grow geometrically when full (capacity 0 -> 1 -> 2 -> 4 ...),
    // then placement-new T(std::forward<Args>(args)...) at mData + mSize.
    // Trap: in v.push_back(v[0]) the argument lives in the old buffer, so
    // construct the new element BEFORE you free the old buffer.
    ((void)args, ...);
    throw std::logic_error("TODO: emplace_back");
  }

  void push_back(const T &v) { emplace_back(v); }
  void push_back(T &&v) { emplace_back(std::move(v)); }

  void pop_back() {
    // TODO: throw std::out_of_range if empty; else destroy the last element.
    throw std::logic_error("TODO: pop_back");
  }

  void resize(size_t n) {
    // TODO: shrink = destroy the tail; grow = reserve(n), then default-construct.
    (void)n;
    throw std::logic_error("TODO: resize");
  }

  void clear() noexcept {
    // TODO: destroy all elements; keep the capacity.
  }

 private:
  // Given: raw, correctly aligned memory. No constructors run here.
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
