// Lab 12: RSS grows steadily until the container is OOM-killed (exit 137,
// no core, no log line: the kernel just SIGKILLs it). Every command is
// "cleaned up" and the in-flight map is empty. There's no `new` anywhere.
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "lablog.h"

class Command : public std::enable_shared_from_this<Command> {
 public:
  explicit Command(uint64_t id) : mId(id), mPayload(4096, 'x') { ++sAlive; }
  ~Command() { --sAlive; }

  void setOnPersisted(std::function<void()> cb) { mOnPersisted = std::move(cb); }
  void onPersisted() {
    if (mOnPersisted) {
      mOnPersisted();
    }
  }
  uint64_t id() const { return mId; }
  static int64_t alive() { return sAlive; }

 private:
  uint64_t mId;
  std::string mPayload;
  std::function<void()> mOnPersisted;
  inline static int64_t sAlive = 0;
};

static long rssKb() {
  std::ifstream status("/proc/self/status");
  std::string key;
  long value = 0;
  while (status >> key) {
    if (key == "VmRSS:") {
      status >> value;
      return value;
    }
  }
  return -1;
}

int main() {
  std::unordered_map<uint64_t, std::shared_ptr<Command>> inflight;
  uint64_t replies = 0;
  for (uint64_t id = 1; id <= 50000; ++id) {
    auto cmd = std::make_shared<Command>(id);
    // Reply to the client once the command is persisted, with its id.
    cmd->setOnPersisted([cmd, &replies] {
      ++replies;
      (void)cmd->id();
    });
    inflight[id] = cmd;

    // ... raft persists it ...
    inflight[id]->onPersisted();
    inflight.erase(id);

    if (id % 10000 == 0) {
      LOG_INFO("processed=%lu inflight=%zu alive Commands=%ld RSS=%ld KB", static_cast<unsigned long>(id),
               inflight.size(), static_cast<long>(Command::alive()), rssKb());
    }
  }
  LOG_INFO("replies=%lu", static_cast<unsigned long>(replies));
}
