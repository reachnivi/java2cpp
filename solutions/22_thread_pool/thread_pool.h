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
    mWorkers.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
      mWorkers.emplace_back([this] { workerLoop(); });
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ~ThreadPool() { shutdown(); }

  // Runs f(args...) on a worker. The future carries the result, or the
  // exception f threw (rethrown by future::get()).
  template <typename F, typename... Args>
  auto submit(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>> {
    using R = std::invoke_result_t<F, Args...>;
    // packaged_task is move-only and std::function needs copyable callables,
    // so the task is held by shared_ptr (or use ex 20's UniqueFunction!).
    auto task = std::make_shared<std::packaged_task<R()>>(
        [fn = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
          return std::apply(std::move(fn), std::move(tup));
        });
    std::future<R> result = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (mStopping) {
        throw std::runtime_error("submit() after shutdown()");
      }
      mQueue.emplace_back([task] { (*task)(); });
    }
    mCv.notify_one();
    return result;
  }

  // Stops accepting work, finishes everything already queued, joins workers.
  // Idempotent. Must not be called from a worker thread (it would join itself).
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (mStopping && mWorkers.empty()) {
        return;
      }
      mStopping = true;
    }
    mCv.notify_all();
    for (auto &w : mWorkers) {
      if (w.joinable()) {
        w.join();
      }
    }
    std::lock_guard<std::mutex> lock(mMutex);
    mWorkers.clear();
  }

  size_t threadCount() const { return mWorkers.size(); }

 private:
  void workerLoop() {
    while (true) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(mMutex);
        mCv.wait(lock, [this] { return mStopping || !mQueue.empty(); });
        if (mQueue.empty()) {
          return;  // stopping and drained
        }
        job = std::move(mQueue.front());
        mQueue.pop_front();
      }
      job();  // never run user code while holding the lock
    }
  }

  std::mutex mMutex;
  std::condition_variable mCv;
  std::deque<std::function<void()>> mQueue;
  std::vector<std::thread> mWorkers;
  bool mStopping = false;
};

}  // namespace j2c

#endif  // J2C_THREAD_POOL_H_
