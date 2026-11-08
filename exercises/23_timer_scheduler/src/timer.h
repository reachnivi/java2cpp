#ifndef J2C_TIMER_H_
#define J2C_TIMER_H_

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace j2c {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using TimerId = uint64_t;

// ---------------------------------------------------------------- TimerQueue
// Single-threaded core: knows nothing about threads or real time. You pass
// "now" in, which makes every behaviour testable with fake time.
class TimerQueue {
 public:
  TimerId schedule(TimePoint when, std::function<void()> cb) {
    // TODO: assign mNextId++, push an Entry{when, mSeq++, id} on the heap,
    // store the callback in mCallbacks.
    (void)when;
    (void)cb;
    return 0;
  }

  // TODO: LAZY cancellation: erase the callback only; popDue/nextDeadline
  // skip heap entries whose id has no callback. (Why not remove it from the
  // heap? Look at std::priority_queue's interface, and think about O(n).)
  bool cancel(TimerId id) {
    (void)id;
    return false;
  }

  // Reset a timer (e.g. Raft's election timeout on every heartbeat).
  // TODO: move the callback to a NEW id scheduled at `when`; return the new
  // id, or 0 if `id` had already fired or been cancelled.
  TimerId reschedule(TimerId id, TimePoint when) {
    (void)id;
    (void)when;
    return 0;
  }

  // TODO: pop every heap entry due at or before `now`; collect the callbacks
  // of live ones (erase each from mCallbacks), in heap order.
  std::vector<std::function<void()>> popDue(TimePoint now) {
    (void)now;
    return {};
  }

  // Single-threaded convenience: pop and run. Callbacks may schedule or
  // cancel timers (a timer scheduled for <= now runs on the NEXT call, which
  // keeps a callback that reschedules itself from looping forever).
  size_t runDue(TimePoint now) {
    auto due = popDue(now);
    for (auto &cb : due) {
      cb();
    }
    return due.size();
  }

  // Earliest live deadline, if any. TODO: pop cancelled entries off the top first.
  bool nextDeadline(TimePoint *out) {
    (void)out;
    return false;
  }

  size_t pending() const { return mCallbacks.size(); }

 private:
  struct Entry {
    TimePoint when;
    uint64_t seq;
    TimerId id;
    // TODO: std::priority_queue is a MAX-heap (top() is the "largest"). Define
    // operator< so the EARLIEST deadline ends up on top, with ties broken by
    // seq so equal deadlines fire in scheduling order.
    bool operator<(const Entry &o) const {
      (void)o;
      return false;
    }
  };
  std::priority_queue<Entry> mHeap;
  std::unordered_map<TimerId, std::function<void()>> mCallbacks;
  TimerId mNextId = 1;
  uint64_t mSeq = 0;
};

// --------------------------------------------------------------- TimerThread
// Runs a TimerQueue on a background thread with real (steady) time.
class TimerThread {
 public:
  TimerThread() : mThread([this] { loop(); }) {}
  ~TimerThread() { stop(); }
  TimerThread(const TimerThread &) = delete;
  TimerThread &operator=(const TimerThread &) = delete;

  TimerId scheduleAfter(std::chrono::milliseconds delay, std::function<void()> cb) {
    TimerId id;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      id = mQueue.schedule(Clock::now() + delay, std::move(cb));
    }
    mCv.notify_one();  // the new timer may be earlier than the one we sleep for
    return id;
  }

  bool cancel(TimerId id) {
    std::lock_guard<std::mutex> lock(mMutex);
    return mQueue.cancel(id);
  }

  TimerId resetAfter(TimerId id, std::chrono::milliseconds delay) {
    TimerId newId;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      newId = mQueue.reschedule(id, Clock::now() + delay);
    }
    mCv.notify_one();
    return newId;
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStopping = true;
    }
    mCv.notify_all();
    if (mThread.joinable()) {
      mThread.join();
    }
  }

 private:
  void loop() {
    // TODO: hold a unique_lock; while not stopping:
    //   - if there's a next deadline: mCv.wait_until(lock, next)
    //     (NOT sleep: scheduleAfter() with an earlier timer, or stop(), must
    //     wake you up), else mCv.wait(lock);
    //   - pop the due callbacks, then UNLOCK and run them. A callback may
    //     call scheduleAfter() on this same TimerThread; holding the lock
    //     would self-deadlock (crash lab 09). Re-lock afterwards.
  }

  std::mutex mMutex;
  std::condition_variable mCv;
  TimerQueue mQueue;
  bool mStopping = false;
  std::thread mThread;  // declared LAST: starts after the members it uses exist
};

}  // namespace j2c

#endif  // J2C_TIMER_H_
