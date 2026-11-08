#ifndef J2C_THREAD_POOL_H_
#define J2C_THREAD_POOL_H_

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace j2c {

// Java: Executors.newFixedThreadPool(n) + submit(Callable) -> Future.
class ThreadPool {
 public:
  explicit ThreadPool(size_t threads) {
    if (threads == 0) {
      throw std::invalid_argument("ThreadPool needs at least one thread");
    }
    // TODO: start `threads` workers, each running workerLoop().
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ~ThreadPool() { shutdown(); }

  // Runs f(args...) on a worker. The future carries the result, or the
  // exception f threw (rethrown by future::get()).
  template <typename F, typename... Args>
  auto submit(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>> {
    using R = std::invoke_result_t<F, Args...>;
    // TODO:
    //  1. Wrap f + args in a std::packaged_task<R()>. Capture the args in a
    //     tuple and call std::apply(std::move(fn), std::move(tup)), so move-only
    //     arguments work.
    //  2. Take its future (task.get_future()) before queueing.
    //  3. Queue a std::function<void()> that runs the task. Trap: packaged_task
    //     is move-only and std::function needs a copyable callable, so hold
    //     the task in a std::shared_ptr (or use your ex 20 UniqueFunction).
    //  4. Under the lock: throw std::runtime_error if shutting down.
    //  5. notify_one.
    ((void)f, ..., (void)args);
    throw std::logic_error("TODO: submit");
    return std::future<R>();
  }

  // Stops accepting work, finishes everything already queued, joins workers.
  // Idempotent. Must not be called from a worker thread (it would join itself).
  void shutdown() {
    // TODO: set mStopping under the lock, notify_all, join every worker.
  }

  size_t threadCount() const { return mWorkers.size(); }

 private:
  void workerLoop() {
    // TODO: loop: wait (with a predicate) for work or stop; when stopping
    // AND the queue is empty, return; otherwise pop one job and run it
    // OUTSIDE the lock (user code must never run while you hold mMutex: why?).
  }

  std::mutex mMutex;
  std::condition_variable mCv;
  std::deque<std::function<void()>> mQueue;
  std::vector<std::thread> mWorkers;
  bool mStopping = false;
};

}  // namespace j2c

#endif  // J2C_THREAD_POOL_H_
