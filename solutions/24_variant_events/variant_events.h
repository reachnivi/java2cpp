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

  ProcessHint process(const Command &cmd, std::vector<Event> *events) const {
    return std::visit(
        overloaded{
            [&](const IncreaseCommand &c) -> ProcessHint {
              if (c.value <= mValue) {
                return {201, "Duplicated request"};
              }
              if (c.value > mValue + 1) {
                return {400, "Invalid request"};
              }
              events->push_back(Increased{c.value});
              return {200, "Success"};
            },
            [&](const DecreaseCommand &c) -> ProcessHint {
              if (c.value < 0) {
                return {400, "value must be non-negative"};
              }
              if (c.value >= mValue) {
                return {201, "Duplicated request"};
              }
              if (c.value < mValue - 1) {
                return {400, "Invalid request"};
              }
              events->push_back(Decreased{c.value});
              return {200, "Success"};
            },
            [&](const ResetCommand &) -> ProcessHint {
              events->push_back(Reset{});
              return {200, "Success"};
            },
        },
        cmd);
  }

  void apply(const Event &e) {
    std::visit(overloaded{
                   [this](const Increased &ev) { mValue = ev.newValue; },
                   [this](const Decreased &ev) { mValue = ev.newValue; },
                   [this](const Reset &) { mValue = 0; },
               },
               e);
  }

 private:
  int mValue = 0;
};

// Wire format: "<index>:<payload>". The index IS the type tag, so the order of
// alternatives in `Event` is a persisted format: append only, never reorder.
inline std::string encode(const Event &e) {
  std::string payload = std::visit(
      [](const auto &ev) -> std::string {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, Reset>) {
          return "";
        } else {
          return std::to_string(ev.newValue);
        }
      },
      e);
  return std::to_string(e.index()) + ":" + payload;
}

inline Event decode(const std::string &s) {
  auto colon = s.find(':');
  if (colon == std::string::npos) {
    throw std::runtime_error("bad event: " + s);
  }
  int index = std::stoi(s.substr(0, colon));
  std::string payload = s.substr(colon + 1);
  switch (index) {
    case 0:
      return Increased{std::stoi(payload)};
    case 1:
      return Decreased{std::stoi(payload)};
    case 2:
      return Reset{};
    default:
      throw std::runtime_error("unknown event index " + std::to_string(index));
  }
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
