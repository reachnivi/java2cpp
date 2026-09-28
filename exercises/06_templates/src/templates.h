#ifndef J2C_TEMPLATES_H_
#define J2C_TEMPLATES_H_

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
    // TODO: false if full; else store at tail, advance tail modulo N.
    (void)value;
    return false;
  }

  std::optional<T> pop() {
    // TODO: std::nullopt if empty; else take from head (std::move it out).
    return std::nullopt;
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
  // TODO: loop over c (range-for works on anything with begin()/end()).
  // Then rewrite with std::count_if from <algorithm>.
  (void)c;
  (void)pred;
  return 0;
}

template <typename T>
std::string describe(const T &v) {
  // TODO: if constexpr (std::is_integral_v<T>) { ... } else if constexpr ...
  // Why `if constexpr` and not plain `if`? Try plain `if` with
  // std::to_string(v) when T = std::vector<int> and read the error.
  (void)v;
  return "";
}

template <typename T>
T maxOf(const T &a, const T &b) {
  return a < b ? b : a;
}

// Variadic overload for 3+ arguments. `Rest...` is a parameter pack.
template <typename T, typename... Rest>
T maxOf(const T &a, const T &b, const Rest &...rest) {
  // TODO: recurse: return maxOf(maxOf(a, b), rest...);
  ((void)rest, ...);  // a C++17 fold expression, here only to silence warnings
  (void)b;
  return a;
}

}  // namespace j2c

#endif  // J2C_TEMPLATES_H_
