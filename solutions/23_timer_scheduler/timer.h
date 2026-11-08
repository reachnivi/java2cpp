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
    TimerId id = mNextId++;
    mHeap.push(Entry{when, mSeq++, id});
    mCallbacks.emplace(id, std::move(cb));
    return id;
  }

  // Lazy cancellation: forget the callback; the heap entry is skipped later.
  // (Removing from the middle of a binary heap is O(n).)
  bool cancel(TimerId id) { return mCallbacks.erase(id) != 0; }

  // Reset a timer (e.g. Raft's election timeout on every heartbeat).
  // Returns the new id, or 0 if `id` had already fired or been cancelled.
  TimerId reschedule(TimerId id, TimePoint when) {
    auto it = mCallbacks.find(id);
    if (it == mCallbacks.end()) {
      return 0;
    }
    auto cb = std::move(it->second);
    mCallbacks.erase(it);
    return schedule(when, std::move(cb));
  }

  // Removes and returns the callbacks of every timer due at or before `now`,
  // in deadline order (ties: scheduling order). Cancelled entries are skipped.
  std::vector<std::function<void()>> popDue(TimePoint now) {
    std::vector<std::function<void()>> due;
    while (!mHeap.empty() && mHeap.top().when <= now) {
      Entry e = mHeap.top();
      mHeap.pop();
      auto it = mCallbacks.find(e.id);
      if (it == mCallbacks.end()) {
        continue;  // cancelled or rescheduled
      }
      due.push_back(std::move(it->second));
      mCallbacks.erase(it);
    }
    return due;
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

  // Earliest live deadline, if any (skips cancelled entries).
  bool nextDeadline(TimePoint *out) {
    while (!mHeap.empty() && mCallbacks.count(mHeap.top().id) == 0) {
      mHeap.pop();
    }
    if (mHeap.empty()) {
      return false;
    }
    *out = mHeap.top().when;
    return true;
  }

  size_t pending() const { return mCallbacks.size(); }

 private:
  struct Entry {
    TimePoint when;
    uint64_t seq;
    TimerId id;
    // std::priority_queue is a MAX-heap: invert so the earliest is on top.
    bool operator<(const Entry &o) const { return when != o.when ? when > o.when : seq > o.seq; }
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
    std::unique_lock<std::mutex> lock(mMutex);
    while (!mStopping) {
      TimePoint next;
      if (mQueue.nextDeadline(&next)) {
        // wait_until, not sleep: a new earlier timer or stop() wakes us.
        mCv.wait_until(lock, next);
      } else {
        mCv.wait(lock);
      }
      if (mStopping) {
        break;
      }
      auto due = mQueue.popDue(Clock::now());
      // Run callbacks WITHOUT the lock: a callback may call scheduleAfter or
      // cancel on this TimerThread (holding the lock would self-deadlock,
      // crash lab 09), and a slow callback must not block scheduling.
      lock.unlock();
      for (auto &cb : due) {
        cb();
      }
      lock.lock();
    }
  }

  std::mutex mMutex;
  std::condition_variable mCv;
  TimerQueue mQueue;
  bool mStopping = false;
  std::thread mThread;  // declared LAST: starts after the members it uses exist
};

}  // namespace j2c

#endif  // J2C_TIMER_H_
