#ifndef J2C_REFS_H_
#define J2C_REFS_H_

#include <charconv>
#include <stdexcept>
#include <string>
#include <vector>

namespace j2c {

inline void increment(int &x) { ++x; }

inline void swapValues(int &a, int &b) {
  int tmp = a;
  a = b;
  b = tmp;
}

inline int sumAll(const std::vector<int> &v) {
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  return sum;
}

inline bool tryParse(const std::string &s, int *out) {
  if (out == nullptr || s.empty()) {
    return false;
  }
  int value = 0;
  const char *end = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(s.data(), end, value);
  if (ec != std::errc() || ptr != end) {
    return false;
  }
  *out = value;
  return true;
}

inline const std::string &longest(const std::vector<std::string> &v) {
  if (v.empty()) {
    throw std::invalid_argument("longest() of empty vector");
  }
  const std::string *best = &v[0];
  for (const auto &s : v) {
    if (s.size() > best->size()) {
      best = &s;
    }
  }
  return *best;
}

inline void appendEvent(std::vector<std::string> *events, const std::string &e) {
  events->push_back(e);
}

}  // namespace j2c

#endif  // J2C_REFS_H_
