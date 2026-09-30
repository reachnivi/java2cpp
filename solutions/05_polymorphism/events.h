#ifndef J2C_EVENTS_H_
#define J2C_EVENTS_H_

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace j2c {

using Type = uint32_t;
using TimestampInNanos = uint64_t;

class Encodable {
 public:
  virtual ~Encodable() = default;
  virtual std::string encodeToString() const = 0;
};

class Event : public Encodable {
 public:
  Event(Type type, TimestampInNanos createdTimeInNanos)
      : mType(type), mCreatedTimeInNanos(createdTimeInNanos) {}

  Type getType() const { return mType; }
  TimestampInNanos getCreatedTimeInNanos() const { return mCreatedTimeInNanos; }

  virtual std::string describe() const { return "Event#" + std::to_string(mType); }

 private:
  Type mType;
  TimestampInNanos mCreatedTimeInNanos;
};

class IncreasedEvent final : public Event {
 public:
  static constexpr Type kType = 1;
  IncreasedEvent(TimestampInNanos ts, int value) : Event(kType, ts), mValue(value) {}

  int getValue() const { return mValue; }

  std::string encodeToString() const override { return "inc:" + std::to_string(mValue); }
  std::string describe() const override { return "Increased to " + std::to_string(mValue); }

 private:
  int mValue;
};

class ResetEvent final : public Event {
 public:
  static constexpr Type kType = 2;
  explicit ResetEvent(TimestampInNanos ts) : Event(kType, ts) {}

  std::string encodeToString() const override { return "reset"; }
};

inline std::unique_ptr<Event> decodeEvent(Type type, const std::string &payload) {
  switch (type) {
    case IncreasedEvent::kType: {
      const std::string prefix = "inc:";
      if (payload.rfind(prefix, 0) != 0) {
        throw std::runtime_error("bad IncreasedEvent payload: " + payload);
      }
      return std::make_unique<IncreasedEvent>(0, std::stoi(payload.substr(prefix.size())));
    }
    case ResetEvent::kType:
      return std::make_unique<ResetEvent>(0);
    default:
      throw std::runtime_error("unknown event type " + std::to_string(type));
  }
}

}  // namespace j2c

#endif  // J2C_EVENTS_H_
