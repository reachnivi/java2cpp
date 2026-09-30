// Lab 15: a production crash. You get: a core file, the stripped binary that
// was deployed (lab15_stripped), and — if the build pipeline kept it — the
// separate debug-info file (lab15.debug). The code is compiled with -O2.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "lablog.h"

struct Account {
  std::string owner;
  int64_t balance;
};

class Ledger {
 public:
  void open(uint64_t id, std::string owner) { mAccounts[id] = Account{std::move(owner), 0}; }

  Account *find(uint64_t id) {
    auto it = mAccounts.find(id);
    return it == mAccounts.end() ? nullptr : &it->second;
  }

  __attribute__((noinline)) int64_t transfer(uint64_t from, uint64_t to, int64_t amount) {
    Account *src = find(from);
    Account *dst = find(to);
    src->balance -= amount;
    dst->balance += amount;
    return dst->balance;
  }

 private:
  std::map<uint64_t, Account> mAccounts;
};

int main(int argc, char **argv) {
  Ledger ledger;
  ledger.open(1, "alice");
  ledger.open(2, "bob");
  uint64_t to = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 7;
  LOG_INFO("transfer 1 -> %lu", static_cast<unsigned long>(to));
  int64_t result = ledger.transfer(1, to, 100);
  std::printf("%ld\n", static_cast<long>(result));
}
