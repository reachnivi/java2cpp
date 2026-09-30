#ifndef J2C_MPSC_QUEUE_H_
#define J2C_MPSC_QUEUE_H_

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace j2c {

class QueueStoppedException : public std::exception {
 public:
  const char *what() const noexcept override { return "Queue has stopped."; }
};

template <typename T>
class BlockingMpscQueue {
 public:
  void enqueue(const T &item) {
    // TODO: lock, throw QueueStoppedException if shut down, push_back,
    // unlock, notify_one. (Notifying after releasing the lock avoids waking
    // the consumer just to block on the mutex.)
    (void)item;
  }

  T dequeue() {
    // TODO: unique_lock + mCondVar.wait(lock, predicate).
    // If queue is empty after waking, we must have been shut down -> throw.
    // Otherwise pop front and return it.
    //
    // The throw below is a placeholder so tests fail fast instead of hanging.
    throw std::logic_error("TODO: implement dequeue");
  }

  uint64_t size() const {
    // TODO: lock (mMutex is `mutable` so a const method can lock it)
    return 0;
  }

  bool empty() const { return size() == 0; }

  void shutdown() {
    // TODO: set flag under lock, notify_all
  }

 private:
  mutable std::mutex mMutex;
  std::condition_variable mCondVar;
  std::deque<T> mQueue;
  bool mShutdown = false;
};

}  // namespace j2c

#endif  // J2C_MPSC_QUEUE_H_
