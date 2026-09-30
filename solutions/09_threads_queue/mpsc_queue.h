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
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (mShutdown) {
        throw QueueStoppedException();
      }
      mQueue.push_back(item);
    }
    mCondVar.notify_one();
  }

  T dequeue() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCondVar.wait(lock, [this] { return !mQueue.empty() || mShutdown; });
    if (mQueue.empty()) {
      throw QueueStoppedException();
    }
    T item = std::move(mQueue.front());
    mQueue.pop_front();
    return item;
  }

  uint64_t size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mQueue.size();
  }

  bool empty() const { return size() == 0; }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mShutdown = true;
    }
    mCondVar.notify_all();
  }

 private:
  mutable std::mutex mMutex;
  std::condition_variable mCondVar;
  std::deque<T> mQueue;
  bool mShutdown = false;
};

}  // namespace j2c

#endif  // J2C_MPSC_QUEUE_H_
