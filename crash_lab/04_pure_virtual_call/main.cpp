// Lab 04: "pure virtual method called" on shutdown.
//
// A reusable Loop base class owns the thread and calls a virtual hook,
// the way app_util's loops call into app-specific subclasses.
#include <atomic>
#include <chrono>
#include <thread>

#include "lablog.h"

class Loop {
 public:
  Loop() = default;
  virtual ~Loop() {
    LOG_INFO("Loop: draining before stop");
    drain();
    stop();
  }

  void start() {
    mThread = std::thread([this] {
      while (mRunning) {
        runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }

  void stop() {
    mRunning = false;
    if (mThread.joinable()) {
      mThread.join();
    }
  }

 protected:
  virtual void runOnce() = 0;

 private:
  void drain() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); }

  std::thread mThread;
  std::atomic<bool> mRunning{true};
};

class EventApplyLoop : public Loop {
 public:
  ~EventApplyLoop() override { LOG_INFO("EventApplyLoop destroyed, applied=%d", mApplied.load()); }

 protected:
  void runOnce() override { ++mApplied; }

 private:
  std::atomic<int> mApplied{0};
};

int main() {
  {
    EventApplyLoop loop;
    loop.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    LOG_INFO("shutting down");
  }
  LOG_INFO("clean exit");
}
