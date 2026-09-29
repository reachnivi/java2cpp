// Lab 11: SIGSEGV on the first event. The backtrace is tens of thousands of
// frames long, and `bt` in gdb seems to scroll forever.
#include <cstdint>
#include <string>

#include "lablog.h"

struct Event {
  explicit Event(uint32_t t) : type(t) {}
  virtual ~Event() = default;
  uint32_t type;
};

struct ProcessedEvent : Event {
  explicit ProcessedEvent(int64_t v) : Event(6), value(v) {}
  int64_t value;
};

class StateMachine {
 public:
  // Generic entry point: dispatch to the typed overload.
  StateMachine &applyEvent(const Event &event) {
    switch (event.type) {
      case 6:
        return apply(event);
      default:
        LOG_WARN("unknown event %u", event.type);
        return *this;
    }
  }

  int64_t value() const { return mValue; }

 private:
  StateMachine &apply(const ProcessedEvent &event) {
    mValue = event.value;
    return *this;
  }

  // Fallback for events without a dedicated handler.
  StateMachine &apply(const Event &event) { return applyEvent(event); }

  int64_t mValue = 0;
};

int main() {
  StateMachine sm;
  ProcessedEvent e(42);
  sm.applyEvent(e);
  LOG_INFO("value=%ld", static_cast<long>(sm.value()));
}
