#ifndef J2C_PERSIST_SERVICE_H_
#define J2C_PERSIST_SERVICE_H_

#include <cstdint>
#include <string>

namespace j2c {

// Interfaces: pure virtual + virtual destructor, so they can be mocked.
class EventStore {
 public:
  virtual ~EventStore() = default;
  virtual uint64_t lastIndex() const = 0;
  virtual bool persist(uint64_t index, const std::string &payload) = 0;
};

class Notifier {
 public:
  virtual ~Notifier() = default;
  virtual void onPersisted(uint64_t commandId, uint64_t index) = 0;
  virtual void onPersistFailed(uint64_t commandId, const std::string &reason) = 0;
};

class PersistService {
 public:
  // Dependencies injected as references: PersistService does NOT own them and
  // they must outlive it. (Java devs: this is constructor injection without a
  // container.)
  PersistService(EventStore &store, Notifier &notifier, int maxAttempts)
      : mStore(store), mNotifier(notifier), mMaxAttempts(maxAttempts) {}

  void submit(uint64_t commandId, const std::string &payload) {
    // TODO: see README.md
    (void)commandId;
    (void)payload;
  }

 private:
  EventStore &mStore;
  Notifier &mNotifier;
  int mMaxAttempts;
};

}  // namespace j2c

#endif  // J2C_PERSIST_SERVICE_H_
