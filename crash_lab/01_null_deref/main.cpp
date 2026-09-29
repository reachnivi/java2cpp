// Lab 01: replaying the event log on startup crashes.
//
// Modelled on Gringofts' CommandDecoderImpl::decodeCommandFromString, which
// logs "Unknown command type" and returns nullptr for types it doesn't know.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "lablog.h"

struct Command {
  virtual ~Command() = default;
  virtual int64_t delta() const = 0;
};

struct IncreaseCommand : Command {
  explicit IncreaseCommand(int64_t d) : mDelta(d) {}
  int64_t delta() const override { return mDelta; }
  int64_t mDelta;
};

struct LogEntry {
  uint64_t index;
  uint32_t type;
  int64_t payload;
};

std::unique_ptr<Command> decodeCommand(const LogEntry &entry) {
  switch (entry.type) {
    case 1:
      return std::make_unique<IncreaseCommand>(entry.payload);
    default:
      LOG_WARN("Unknown command type: %u", entry.type);
      return nullptr;
  }
}

class StateMachine {
 public:
  void apply(const Command &cmd) { mValue += cmd.delta(); }
  int64_t value() const { return mValue; }

 private:
  int64_t mValue = 0;
};

void replayOne(StateMachine *sm, const LogEntry &entry) {
  auto cmd = decodeCommand(entry);
  sm->apply(*cmd);
}

void recover(StateMachine *sm, const std::vector<LogEntry> &log) {
  for (const auto &entry : log) {
    replayOne(sm, entry);
  }
}

int main() {
  // A newer version of the app wrote a type-2 command; this binary is older.
  std::vector<LogEntry> log = {
      {1, 1, 10}, {2, 1, 5}, {3, 1, -3}, {4, 2, 7}, {5, 1, 1},
  };
  StateMachine sm;
  LOG_INFO("recovering %zu entries", log.size());
  recover(&sm, log);
  LOG_INFO("recovered, value=%ld", static_cast<long>(sm.value()));
}
