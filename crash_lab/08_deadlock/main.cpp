// Lab 08: the process hangs. CPU is 0%, no log lines, no crash, health
// checks time out. `kill` (SIGTERM) does nothing useful here.
//
// Two paths touch both the state machine and the raft log: the apply path
// and the snapshot path.
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include "lablog.h"

class Node {
 public:
  // Apply loop: take the log lock to read the next entry, then the state lock.
  void applyNext() {
    std::lock_guard<std::mutex> logLock(mLogMutex);
    uint64_t entry = mLastLogIndex;
    simulateWork();
    std::lock_guard<std::mutex> stateLock(mStateMutex);
    mAppliedIndex = entry;
  }

  // Snapshot: freeze the state, then truncate the log up to the applied index.
  void takeSnapshot() {
    std::lock_guard<std::mutex> stateLock(mStateMutex);
    uint64_t upTo = mAppliedIndex;
    simulateWork();
    std::lock_guard<std::mutex> logLock(mLogMutex);
    mFirstLogIndex = upTo;
  }

  void append() {
    std::lock_guard<std::mutex> logLock(mLogMutex);
    ++mLastLogIndex;
  }

 private:
  static void simulateWork() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }

  std::mutex mLogMutex;
  std::mutex mStateMutex;
  uint64_t mFirstLogIndex = 0;
  uint64_t mLastLogIndex = 0;
  uint64_t mAppliedIndex = 0;
};

int main() {
  Node node;
  LOG_INFO("pid %d starting", static_cast<int>(::getpid()));
  std::thread applyLoop([&node] {
    for (int i = 0; i < 50; ++i) {
      node.append();
      node.applyNext();
    }
  });
  std::thread snapshotter([&node] {
    for (int i = 0; i < 50; ++i) {
      node.takeSnapshot();
    }
  });
  applyLoop.join();
  snapshotter.join();
  LOG_INFO("done");
}
