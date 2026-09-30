// Lab 06: after the cluster scales out, the leader hint is garbage, empty,
// or the process segfaults while logging it. Which one you get depends on the
// compiler, the flags and the allocator: classic undefined behaviour.
#include <cstdint>
#include <string>
#include <vector>

#include "lablog.h"

struct Peer {
  uint64_t id;
  std::string addr;
};

class ClusterInfo {
 public:
  void addPeer(uint64_t id, const std::string &addr) { mPeers.push_back(Peer{id, addr}); }
  const Peer &peer(size_t i) const { return mPeers[i]; }

 private:
  std::vector<Peer> mPeers;
};

int main() {
  ClusterInfo cluster;
  cluster.addPeer(1, "10.0.0.1:5254-with-a-long-hostname-that-defeats-SSO.example.com");

  // Cache the leader; it's used in every "not leader" reply.
  const Peer &leader = cluster.peer(0);
  LOG_INFO("leader hint: %lu@%s", static_cast<unsigned long>(leader.id), leader.addr.c_str());

  // Scale out.
  for (uint64_t i = 2; i <= 9; ++i) {
    cluster.addPeer(i, "10.0.0." + std::to_string(i) + ":5254-another-long-hostname.example.com");
  }

  LOG_INFO("leader hint after scale-out: %lu@%s", static_cast<unsigned long>(leader.id), leader.addr.c_str());
}
