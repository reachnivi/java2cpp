#ifndef J2C_REFS_H_
#define J2C_REFS_H_

#include <stdexcept>
#include <string>
#include <vector>

namespace j2c {

inline void increment(int &x) {
  // TODO
  (void)x;
}

inline void swapValues(int &a, int &b) {
  // TODO (then look up std::swap, which is what you'd really use)
  (void)a;
  (void)b;
}

inline int sumAll(const std::vector<int> &v) {
  // TODO: range-for loop
  (void)v;
  return 0;
}

inline bool tryParse(const std::string &s, int *out) {
  // TODO: return false on nullptr / invalid input, otherwise write *out.
  (void)s;
  (void)out;
  return false;
}

inline const std::string &longest(const std::vector<std::string> &v) {
  // TODO: return a reference to the longest element. Throw
  // std::invalid_argument when v is empty.
  static const std::string kPlaceholder;  // delete me
  (void)v;
  return kPlaceholder;
}

inline void appendEvent(std::vector<std::string> *events, const std::string &e) {
  // TODO
  (void)events;
  (void)e;
}

}  // namespace j2c

#endif  // J2C_REFS_H_
