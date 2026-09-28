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
  // Given to you: without a virtual destructor, `delete basePtr` on a derived
  // object is undefined behaviour. (It also makes the class polymorphic, which
  // dynamic_cast needs.) See the Experiments section of the README.
  virtual ~Encodable() = default;

  // TODO: turn this into a pure virtual function:
  //   virtual std::string encodeToString() const = 0;
  std::string encodeToString() const { return ""; }
};

// TODO: Event must stay abstract (it doesn't implement encodeToString).
class Event : public Encodable {
 public:
  Event(Type type, TimestampInNanos createdTimeInNanos)
      : mType(type), mCreatedTimeInNanos(createdTimeInNanos) {}

  Type getType() const { return mType; }
  TimestampInNanos getCreatedTimeInNanos() const { return mCreatedTimeInNanos; }

  // TODO: make this virtual so subclasses can override it.
  std::string describe() const { return "Event#" + std::to_string(mType); }

 private:
  Type mType;
  TimestampInNanos mCreatedTimeInNanos;
};

class IncreasedEvent : public Event {  // TODO: mark final
 public:
  static constexpr Type kType = 1;
  IncreasedEvent(TimestampInNanos ts, int value) : Event(kType, ts), mValue(value) {}

  int getValue() const { return mValue; }

  // TODO: override encodeToString() -> "inc:<value>"
  // TODO: override describe()       -> "Increased to <value>"
  std::string encodeToString() const { return ""; }

 private:
  int mValue;
};

class ResetEvent final : public Event {
 public:
  static constexpr Type kType = 2;
  explicit ResetEvent(TimestampInNanos ts) : Event(kType, ts) {}

  // TODO: override encodeToString() -> "reset"
  std::string encodeToString() const { return ""; }
};

inline std::unique_ptr<Event> decodeEvent(Type type, const std::string &payload) {
  // TODO: switch on type. For IncreasedEvent parse the int after "inc:".
  // Unknown type -> throw std::runtime_error. Use timestamp 0.
  (void)type;
  (void)payload;
  return nullptr;
}

}  // namespace j2c

#endif  // J2C_EVENTS_H_
