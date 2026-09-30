// Lab 14: a freshly started follower occasionally believes it is the leader
// and starts accepting writes (split brain). The Debug build never does it.
// The Release build does it "sometimes". Adding a log line makes it go away.
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "lablog.h"

class RaftState {
 public:
  explicit RaftState(uint64_t selfId) : mSelfId(selfId) {}

  bool isLeader() const { return mIsLeader; }
  uint64_t currentTerm() const { return mCurrentTerm; }
  uint64_t selfId() const { return mSelfId; }

 private:
  uint64_t mSelfId;
  uint64_t mCurrentTerm = 0;
  bool mIsLeader;  // set by becomeLeader()/becomeFollower() later
};

// Simulates earlier heap activity leaving non-zero bytes behind, as a
// long-running server's heap would.
static void dirtyTheHeap() {
  for (int i = 0; i < 100; ++i) {
    auto *junk = new char[sizeof(RaftState)];
    std::memset(junk, 0xAB, sizeof(RaftState));
    delete[] junk;
  }
}

int main() {
  dirtyTheHeap();
  auto state = std::make_unique<RaftState>(3);
  if (state->isLeader()) {
    LOG_ERROR("node %lu thinks it is LEADER at term %lu without an election!",
              static_cast<unsigned long>(state->selfId()), static_cast<unsigned long>(state->currentTerm()));
    return 2;
  }
  LOG_INFO("node starts as follower");
}
