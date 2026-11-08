#ifndef J2C_VARIANT_EVENTS_H_
#define J2C_VARIANT_EVENTS_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace j2c::v {

// The ex 12 capstone again, but events are a closed set of plain structs in a
// std::variant instead of a class hierarchy with virtual functions.

struct Increased {
  int newValue;
};
struct Decreased {
  int newValue;
};
struct Reset {};

using Event = std::variant<Increased, Decreased, Reset>;

struct IncreaseCommand {
  int value;
};
struct DecreaseCommand {
  int value;
};
struct ResetCommand {};

using Command = std::variant<IncreaseCommand, DecreaseCommand, ResetCommand>;

struct ProcessHint {
  uint32_t mCode;
  std::string mMessage;
};

// The classic helper: build one visitor out of several lambdas.
template <typename... Fs>
struct overloaded : Fs... {
  using Fs::operator()...;
};
template <typename... Fs>
overloaded(Fs...) -> overloaded<Fs...>;  // deduction guide (C++17)

class StateMachine {
 public:
  int value() const { return mValue; }

  // TODO: std::visit(overloaded{ lambda per command type ... }, cmd).
  // Same rules as ex 12 (IncreaseHandler semantics); ResetCommand always
  // emits Reset{}. Note: process() is const: it decides, it never mutates.
  ProcessHint process(const Command &cmd, std::vector<Event> *events) const {
    (void)cmd;
    (void)events;
    return {0, "TODO"};
  }

  // TODO: std::visit with one lambda per event type. Then try deleting the
  // Reset lambda and read the compiler error: that's exhaustiveness checking,
  // which a virtual-function hierarchy can't give you.
  void apply(const Event &e) { (void)e; }

 private:
  int mValue = 0;
};

// Wire format: "<index>:<payload>". The index IS the type tag, so the order of
// alternatives in `Event` is a persisted format: append only, never reorder.
// TODO: e.index() + ":" + payload. A generic lambda with `if constexpr` on
// std::decay_t<decltype(ev)> handles "has newValue" vs Reset in one visitor.
inline std::string encode(const Event &e) {
  (void)e;
  return "TODO";
}

// TODO: split at ':', switch on the index, construct the alternative.
// Throw std::runtime_error for a missing ':' or an unknown index.
inline Event decode(const std::string &s) {
  (void)s;
  throw std::logic_error("TODO: decode");
}

inline StateMachine replay(const std::vector<std::string> &log) {
  StateMachine sm;
  for (const auto &entry : log) {
    sm.apply(decode(entry));
  }
  return sm;
}

}  // namespace j2c::v

#endif  // J2C_VARIANT_EVENTS_H_
