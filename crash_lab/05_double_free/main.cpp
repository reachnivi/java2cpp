// Lab 05: glibc aborts with "free(): invalid pointer" (or "double free
// detected") after the first command completes.
//
// Commands register themselves with a tracker so a metrics thread can see
// in-flight work (think Command + RequestHandle bookkeeping in Gringofts).
#include <memory>
#include <string>
#include <unordered_map>

#include "lablog.h"

class Command;

class InflightTracker {
 public:
  void add(uint64_t id, std::shared_ptr<Command> cmd) { mInflight[id] = std::move(cmd); }
  void remove(uint64_t id) { mInflight.erase(id); }
  size_t size() const { return mInflight.size(); }

 private:
  std::unordered_map<uint64_t, std::shared_ptr<Command>> mInflight;
};

class Command {
 public:
  Command(uint64_t id, std::string payload) : mId(id), mPayload(std::move(payload)) {}

  void registerWith(InflightTracker *tracker) {
    // "I need a shared_ptr to myself to hand to the tracker."
    tracker->add(mId, std::shared_ptr<Command>(this));
  }

  uint64_t id() const { return mId; }

 private:
  uint64_t mId;
  std::string mPayload;
};

int main() {
  InflightTracker tracker;
  auto cmd = std::make_shared<Command>(1, "increase:1");
  cmd->registerWith(&tracker);
  LOG_INFO("in flight: %zu", tracker.size());

  // Command persisted; stop tracking it.
  tracker.remove(cmd->id());
  LOG_INFO("in flight: %zu", tracker.size());
}
