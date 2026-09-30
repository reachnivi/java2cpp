#ifndef J2C_LOOP_H_
#define J2C_LOOP_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace j2c {

class Metrics {
 public:
  void recordProcessed() {
    // TODO: make mProcessed atomic and use fetch_add(1, std::memory_order_relaxed)
  }
  uint64_t processed() const { return mProcessed; }

 private:
  uint64_t mProcessed = 0;  // TODO: std::atomic<uint64_t>
};

class Worker {
 public:
  Worker() = default;
  ~Worker() {
    // TODO: stop();
  }
  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;

  void start(std::function<void()> tick, std::chrono::milliseconds interval) {
    // TODO:
    //  - throw std::logic_error if already running
    //  - set mRunning, spawn mThread = std::thread([this, tick, interval] { ... })
    //  - loop body: tick(); then wait on mCondVar for `interval` or until stopped:
    //      std::unique_lock<std::mutex> lock(mMutex);
    //      mCondVar.wait_for(lock, interval, [this] { return !mRunning; });
    (void)tick;
    (void)interval;
  }

  void stop() {
    // TODO: flip mRunning under the mutex, notify, join if joinable.
  }

  bool isRunning() const { return mRunning; }

 private:
  std::thread mThread;
  std::atomic<bool> mRunning{false};
  std::mutex mMutex;
  std::condition_variable mCondVar;
};

}  // namespace j2c

#endif  // J2C_LOOP_H_
