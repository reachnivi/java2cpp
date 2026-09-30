#ifndef J2C_PERSIST_SERVICE_H_
#define J2C_PERSIST_SERVICE_H_

#include <cstdint>
#include <string>

namespace j2c {

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
  PersistService(EventStore &store, Notifier &notifier, int maxAttempts)
      : mStore(store), mNotifier(notifier), mMaxAttempts(maxAttempts) {}

  void submit(uint64_t commandId, const std::string &payload) {
    if (payload.empty()) {
      mNotifier.onPersistFailed(commandId, "empty payload");
      return;
    }
    const uint64_t index = mStore.lastIndex() + 1;
    for (int attempt = 0; attempt < mMaxAttempts; ++attempt) {
      if (mStore.persist(index, payload)) {
        mNotifier.onPersisted(commandId, index);
        return;
      }
    }
    mNotifier.onPersistFailed(commandId,
                              "persist failed after " + std::to_string(mMaxAttempts) + " attempts");
  }

 private:
  EventStore &mStore;
  Notifier &mNotifier;
  int mMaxAttempts;
};

}  // namespace j2c

#endif  // J2C_PERSIST_SERVICE_H_
