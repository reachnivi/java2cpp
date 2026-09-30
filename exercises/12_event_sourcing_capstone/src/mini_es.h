#ifndef J2C_MINI_ES_H_
#define J2C_MINI_ES_H_

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace j2c::es {  // C++17 nested namespace syntax

using Type = uint32_t;

struct ProcessHint {
  uint32_t mCode;
  std::string mMessage;
};

// ------------------------------------------------------------------ Events

class Event {
 public:
  explicit Event(Type type) : mType(type) {}
  virtual ~Event() = default;
  Type getType() const { return mType; }
  virtual std::string encodeToString() const = 0;

 private:
  Type mType;
};

/// Emitted by IncreaseCommand. Same name as Gringofts' demo event.
class ProcessedEvent final : public Event {
 public:
  static constexpr Type kType = 1;
  explicit ProcessedEvent(int newValue) : Event(kType), mNewValue(newValue) {}
  int getNewValue() const { return mNewValue; }
  std::string encodeToString() const override { return std::to_string(mNewValue); }

 private:
  int mNewValue;
};

// TODO(feature): implement like ProcessedEvent, with kType = 2.
class DecreasedEvent final : public Event {
 public:
  static constexpr Type kType = 2;
  explicit DecreasedEvent(int newValue) : Event(kType), mNewValue(newValue) {}
  int getNewValue() const { return mNewValue; }
  std::string encodeToString() const override { return "TODO"; }

 private:
  int mNewValue;
};

inline std::unique_ptr<Event> decodeEvent(Type type, const std::string &payload) {
  switch (type) {
    case ProcessedEvent::kType:
      return std::make_unique<ProcessedEvent>(std::stoi(payload));
    // TODO(feature): decode DecreasedEvent
    default:
      throw std::runtime_error("unknown event type " + std::to_string(type));
  }
}

// ---------------------------------------------------------------- Commands

class Command {
 public:
  static constexpr const char *kVerifiedSuccess = "Success";

  explicit Command(Type type) : mType(type) {}
  virtual ~Command() = default;
  Type getType() const { return mType; }
  virtual std::string verifyCommand() const { return kVerifiedSuccess; }

 private:
  Type mType;
};

class IncreaseCommand final : public Command {
 public:
  static constexpr Type kType = 1;
  explicit IncreaseCommand(int value) : Command(kType), mValue(value) {}
  int getValue() const { return mValue; }

 private:
  int mValue;
};

class DecreaseCommand final : public Command {
 public:
  static constexpr Type kType = 2;
  explicit DecreaseCommand(int value) : Command(kType), mValue(value) {}
  int getValue() const { return mValue; }

  // TODO(feature): override verifyCommand(): negative -> "value must be non-negative"

 private:
  int mValue;
};

// ----------------------------------------------------------- State machine

class StateMachine {
 public:
  int getValue() const { return mValue; }

  /// Pure decision: looks at state + command, emits events, never mutates state.
  ProcessHint processCommand(const Command &command, std::vector<std::shared_ptr<Event>> *events) const {
    switch (command.getType()) {
      case IncreaseCommand::kType:
        return processIncrease(static_cast<const IncreaseCommand &>(command), events);
      // TODO(feature): route DecreaseCommand to processDecrease.
      // static_cast is safe here because getType() told us the dynamic type
      // (Gringofts does the same; dynamic_cast would also work, but costs more).
      default:
        return {400, "Unknown command type"};
    }
  }

  /// The only way state changes. Must be deterministic.
  StateMachine &applyEvent(const Event &event) {
    switch (event.getType()) {
      case ProcessedEvent::kType:
        mValue = static_cast<const ProcessedEvent &>(event).getNewValue();
        break;
      // TODO(feature): apply DecreasedEvent
      default:
        throw std::runtime_error("cannot apply event type " + std::to_string(event.getType()));
    }
    return *this;
  }

  bool hasSameState(const StateMachine &other) const { return mValue == other.mValue; }

 private:
  ProcessHint processIncrease(const IncreaseCommand &cmd, std::vector<std::shared_ptr<Event>> *events) const {
    if (mValue >= cmd.getValue()) {
      return {201, "Duplicated request"};
    }
    if (mValue + 1 < cmd.getValue()) {
      return {400, "Invalid request"};
    }
    events->push_back(std::make_shared<ProcessedEvent>(cmd.getValue()));
    return {200, "Success"};
  }

  ProcessHint processDecrease(const DecreaseCommand &cmd, std::vector<std::shared_ptr<Event>> *events) const {
    // TODO(feature): see README for the rules. Must NOT modify mValue (this is
    // a const member function: the compiler enforces it).
    (void)cmd;
    (void)events;
    return {500, "TODO"};
  }

  int mValue = 0;
};

// ------------------------------------------------------------- Event store

/// Stands in for Gringofts' CommandEventStore / Raft log: stores encoded events.
class EventLog {
 public:
  struct Entry {
    Type type;
    std::string payload;
  };

  void append(const Event &event) { mEntries.push_back({event.getType(), event.encodeToString()}); }
  const std::vector<Entry> &entries() const { return mEntries; }
  std::size_t size() const { return mEntries.size(); }

 private:
  std::vector<Entry> mEntries;
};

// --------------------------------------------------------------------- App

class App {
 public:
  explicit App(EventLog &log) : mLog(log) {}

  ProcessHint handle(const Command &command) {
    auto verified = command.verifyCommand();
    if (verified != Command::kVerifiedSuccess) {
      return {400, verified};
    }
    std::vector<std::shared_ptr<Event>> events;
    auto hint = mState.processCommand(command, &events);
    if (hint.mCode != 200) {
      return hint;
    }
    for (const auto &e : events) {
      mLog.append(*e);  // persist first ...
    }
    for (const auto &e : events) {
      mState.applyEvent(*e);  // ... then apply
    }
    return hint;
  }

  const StateMachine &state() const { return mState; }

  static StateMachine recover(const EventLog &log) {
    // TODO(feature): decode each entry and apply it to a fresh StateMachine.
    (void)log;
    return StateMachine();
  }

 private:
  EventLog &mLog;
  StateMachine mState;
};

}  // namespace j2c::es

#endif  // J2C_MINI_ES_H_
