// Lab 02: the service crashes every time it is shut down (SIGTERM / rolling
// restart). Nobody notices in dev because they kill it with Ctrl-C twice.
//
// Modelled on Gringofts' MpscQueue contract: dequeue() throws
// QueueStoppedException once the queue is shut down.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "lablog.h"

class QueueStoppedException : public std::exception {
 public:
  const char *what() const noexcept override { return "Queue has stopped."; }
};

template <typename T>
class BlockingQueue {
 public:
  void enqueue(T item) {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mQueue.push_back(std::move(item));
    }
    mCv.notify_one();
  }
  T dequeue() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCv.wait(lock, [this] { return !mQueue.empty() || mStopped; });
    if (mQueue.empty()) {
      throw QueueStoppedException();
    }
    T item = std::move(mQueue.front());
    mQueue.pop_front();
    return item;
  }
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStopped = true;
    }
    mCv.notify_all();
  }

 private:
  std::mutex mMutex;
  std::condition_variable mCv;
  std::deque<T> mQueue;
  bool mStopped = false;
};

class CommandProcessLoop {
 public:
  explicit CommandProcessLoop(BlockingQueue<std::string> *queue) : mQueue(queue) {}

  void run() {
    while (mRunning) {
      auto cmd = mQueue->dequeue();
      process(cmd);
    }
  }
  void shutdown() { mRunning = false; }

 private:
  void process(const std::string &cmd) { LOG_INFO("processed %s", cmd.c_str()); }

  BlockingQueue<std::string> *mQueue;
  std::atomic<bool> mRunning{true};
};

int main() {
  BlockingQueue<std::string> queue;
  CommandProcessLoop loop(&queue);
  std::thread loopThread([&loop] { loop.run(); });

  for (int i = 0; i < 3; ++i) {
    queue.enqueue("cmd-" + std::to_string(i));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LOG_INFO("shutting down");
  queue.shutdown();
  loop.shutdown();
  loopThread.join();
  LOG_INFO("clean exit");
}
