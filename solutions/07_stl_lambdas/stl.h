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
  std::map<std::string, double> totals;
  for (const auto &o : orders) {
    totals[o.user] += o.amount;
  }
  return totals;
}

inline std::vector<int> idsAbove(const std::vector<Order> &orders, double threshold) {
  std::vector<int> ids;
  for (const auto &o : orders) {
    if (o.amount > threshold) {
      ids.push_back(o.id);
    }
  }
  return ids;
}

inline std::vector<Order> topN(std::vector<Order> orders, std::size_t n) {
  n = std::min(n, orders.size());
  std::partial_sort(orders.begin(), orders.begin() + static_cast<std::ptrdiff_t>(n), orders.end(),
                    [](const Order &a, const Order &b) { return a.amount > b.amount; });
  orders.resize(n);
  return orders;
}

inline void removeSmall(std::vector<Order> *orders, double min) {
  orders->erase(std::remove_if(orders->begin(), orders->end(),
                               [min](const Order &o) { return o.amount < min; }),
                orders->end());
}

inline std::vector<std::string> distinctUsersSorted(const std::vector<Order> &orders) {
  std::set<std::string> users;
  for (const auto &o : orders) {
    users.insert(o.user);
  }
  return {users.begin(), users.end()};
}

inline std::function<int()> makeCounter() {
  return [count = 0]() mutable { return ++count; };
}

inline std::unordered_map<std::string, int> countWords(const std::string &text) {
  std::unordered_map<std::string, int> counts;
  std::istringstream in(text);
  std::string word;
  while (in >> word) {
    ++counts[word];
  }
  return counts;
}

}  // namespace j2c

#endif  // J2C_STL_H_
