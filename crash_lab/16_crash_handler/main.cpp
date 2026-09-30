// Lab 16: in production you often have no core file (core dumps disabled,
// container gone, disk full) — only logs. A crash handler that writes a stack
// trace to the log is your insurance. Gringofts doesn't install one.
//
// This program installs one (see crash_handler.h), then crashes. Your job is
// to turn the raw addresses it prints into file:line with addr2line.
#include <cstdint>
#include <memory>
#include <vector>

#include "crash_handler.h"
#include "lablog.h"

struct Peer {
  uint64_t id;
  uint64_t matchIndex;
};

class ReplicationTracker {
 public:
  explicit ReplicationTracker(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      mPeers.push_back(std::make_unique<Peer>(Peer{i + 1, 0}));
    }
  }

  void onAppendEntriesReply(uint64_t peerId, uint64_t matchIndex) {
    // peer ids are 1-based; but the one that just joined isn't in mPeers yet
    Peer *peer = peerId <= mPeers.size() ? mPeers[peerId - 1].get() : nullptr;
    peer->matchIndex = matchIndex;
  }

 private:
  std::vector<std::unique_ptr<Peer>> mPeers;
};

int main() {
  installCrashHandler();
  ReplicationTracker tracker(3);
  tracker.onAppendEntriesReply(2, 10);
  LOG_INFO("peer 2 caught up; now a reply from the newly added peer 4");
  tracker.onAppendEntriesReply(4, 10);
  LOG_INFO("unreachable");
}
