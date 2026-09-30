// Lab 09: the first request that hits the "become leader" path hangs forever.
// Single-threaded program. In Java this code would work.
#include <cstdint>
#include <mutex>
#include <string>

#include "lablog.h"

class RaftRole {
 public:
  uint64_t term() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTerm;
  }

  std::string describe() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mRole + "@term" + std::to_string(mTerm);
  }

  void becomeLeader() {
    std::lock_guard<std::mutex> lock(mMutex);
    mRole = "leader";
    ++mTerm;
    LOG_INFO("now %s (term %lu)", mRole.c_str(), static_cast<unsigned long>(term()));
  }

 private:
  mutable std::mutex mMutex;
  std::string mRole = "follower";
  uint64_t mTerm = 1;
};

int main() {
  RaftRole role;
  LOG_INFO("start: %s", role.describe().c_str());
  role.becomeLeader();
  LOG_INFO("end: %s", role.describe().c_str());
}
