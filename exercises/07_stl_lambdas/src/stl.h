#ifndef J2C_STL_H_
#define J2C_STL_H_

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace j2c {

struct Order {
  int id;
  std::string user;
  double amount;
};

inline std::map<std::string, double> totalByUser(const std::vector<Order> &orders) {
  // TODO: `totals[o.user] += o.amount;` works because operator[] default-inserts 0.0
  (void)orders;
  return {};
}

inline std::vector<int> idsAbove(const std::vector<Order> &orders, double threshold) {
  // TODO
  (void)orders;
  (void)threshold;
  return {};
}

inline std::vector<Order> topN(std::vector<Order> orders, std::size_t n) {
  // Note: `orders` is taken BY VALUE on purpose: we need a copy to sort anyway,
  // and a caller passing a temporary gets it moved in for free.
  // TODO: sort descending by amount with a lambda, then resize to min(n, size).
  (void)orders;
  (void)n;
  return {};
}

inline void removeSmall(std::vector<Order> *orders, double min) {
  // TODO: orders->erase(std::remove_if(...), orders->end());
  (void)orders;
  (void)min;
}

inline std::vector<std::string> distinctUsersSorted(const std::vector<Order> &orders) {
  // TODO: std::set, or sort + std::unique + erase
  (void)orders;
  return {};
}

inline std::function<int()> makeCounter() {
  // TODO: return [count = 0]() mutable { return ++count; };
  return nullptr;
}

inline std::unordered_map<std::string, int> countWords(const std::string &text) {
  // TODO: std::istringstream in(text); std::string w; while (in >> w) ...
  (void)text;
  return {};
}

}  // namespace j2c

#endif  // J2C_STL_H_
