#ifndef J2C_TEMPLATES_H_
#define J2C_TEMPLATES_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

namespace j2c {

template <typename T, std::size_t N>
class RingBuffer {
 public:
  static constexpr std::size_t capacity() { return N; }

  bool push(T value) {
    if (full()) {
      return false;
    }
    mItems[(mHead + mSize) % N] = std::move(value);
    ++mSize;
    return true;
  }

  std::optional<T> pop() {
    if (empty()) {
      return std::nullopt;
    }
    T value = std::move(mItems[mHead]);
    mHead = (mHead + 1) % N;
    --mSize;
    return value;
  }

  std::size_t size() const { return mSize; }
  bool empty() const { return mSize == 0; }
  bool full() const { return mSize == N; }

 private:
  std::array<T, N> mItems{};
  std::size_t mHead = 0;
  std::size_t mSize = 0;
};

template <typename Container, typename Pred>
std::size_t countIf(const Container &c, Pred pred) {
  return static_cast<std::size_t>(std::count_if(std::begin(c), std::end(c), pred));
}

template <typename T>
std::string describe(const T &v) {
  if constexpr (std::is_integral_v<T>) {
    return "int:" + std::to_string(v);
  } else if constexpr (std::is_floating_point_v<T>) {
    return "float";
  } else if constexpr (std::is_same_v<T, std::string>) {
    return "string:" + v;
  } else {
    return "other";
  }
}

template <typename T>
T maxOf(const T &a, const T &b) {
  return a < b ? b : a;
}

template <typename T, typename... Rest>
T maxOf(const T &a, const T &b, const Rest &...rest) {
  return maxOf(maxOf(a, b), rest...);
}

}  // namespace j2c

#endif  // J2C_TEMPLATES_H_
