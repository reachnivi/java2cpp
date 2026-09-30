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
  void recordProcessed() { mProcessed.fetch_add(1, std::memory_order_relaxed); }
  uint64_t processed() const { return mProcessed.load(); }

 private:
  std::atomic<uint64_t> mProcessed{0};
};

class Worker {
 public:
  Worker() = default;
  ~Worker() { stop(); }
  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;

  void start(std::function<void()> tick, std::chrono::milliseconds interval) {
    if (mRunning.exchange(true)) {
      throw std::logic_error("Worker already running");
    }
    mThread = std::thread([this, tick = std::move(tick), interval] {
      while (mRunning) {
        tick();
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait_for(lock, interval, [this] { return !mRunning; });
      }
    });
  }

  void stop() {
    {
      // Holding the mutex while flipping the flag guarantees the worker is
      // either not yet waiting (and will see the flag) or already waiting (and
      // will get the notify). Without it, the notify could be lost.
      std::lock_guard<std::mutex> lock(mMutex);
      mRunning = false;
    }
    mCondVar.notify_all();
    if (mThread.joinable()) {
      mThread.join();
    }
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
