// Lab 10: rare crashes in production (inside std::unordered_map or
// std::string code), wrong peer counts in metrics, and "it never happens
// on my laptop". Run it several times; it may crash, hang, print a wrong
// number, or look perfectly fine.
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "lablog.h"

class PeerTable {
 public:
  void heartbeat(const std::string &peer) { mLastSeen[peer]++; }
  size_t size() const { return mLastSeen.size(); }
  uint64_t totalHeartbeats() const {
    uint64_t total = 0;
    for (const auto &kv : mLastSeen) {
      total += kv.second;
    }
    return total;
  }

 private:
  std::unordered_map<std::string, uint64_t> mLastSeen;
};

int main() {
  PeerTable table;
  constexpr int kThreads = 4;
  constexpr int kBeats = 20000;
  std::vector<std::thread> rpcThreads;  // one per gRPC completion queue
  for (int t = 0; t < kThreads; ++t) {
    rpcThreads.emplace_back([&table, t] {
      for (int i = 0; i < kBeats; ++i) {
        table.heartbeat("peer-" + std::to_string((i * 7 + t) % 64));
      }
    });
  }
  for (auto &t : rpcThreads) {
    t.join();
  }
  LOG_INFO("peers=%zu (expected 64) heartbeats=%lu (expected %d)", table.size(),
           static_cast<unsigned long>(table.totalHeartbeats()), kThreads * kBeats);
}
