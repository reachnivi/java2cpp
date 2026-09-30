#ifndef J2C_ASYNC_CQ_H_
#define J2C_ASYNC_CQ_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace j2c::rpc {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class StatusCode { OK, DEADLINE_EXCEEDED, CANCELLED };

struct Request {
  uint64_t id = 0;
  int64_t value = 0;
  TimePoint deadline = TimePoint::max();
};

struct Response {
  uint64_t id = 0;
  StatusCode status = StatusCode::OK;
  int64_t result = 0;
};

// ------------------------------------------------------------ YOUR CODE (1)
// Same contract as grpc::CompletionQueue:
//   post()  : an operation completed; deliver (tag, ok) to next().
//   next()  : blocks until an event is available; returns true with it.
//             After shutdown(), keeps returning queued events, then false.
//   shutdown(): no more post() allowed (throws std::logic_error).
class CompletionQueue {
 public:
  void post(void *tag, bool ok) {
    // TODO: under the mutex: throw std::logic_error if shut down; push (tag, ok).
    // Then notify one waiter.
    (void)tag;
    (void)ok;
  }

  bool next(void **tag, bool *ok) {
    // TODO: wait (with a predicate!) for an event or shutdown. Drain queued
    // events even after shutdown; return false only when shut down AND empty.
    (void)tag;
    (void)ok;
    throw std::logic_error("TODO: CompletionQueue::next");
  }

  void shutdown() {
    // TODO: set the flag under the mutex, notify all.
  }

 private:
  std::mutex mMutex;
  std::condition_variable mCv;
  std::deque<std::pair<void *, bool>> mEvents;
  bool mShutdown = false;
};

// ------------------------------------------------------------ GIVEN
// Plays the role of grpc::Server + the generated AsyncService: it owns the
// "network". Clients inject() requests; the server matches them to calls
// that requestCall() is waiting on, like service->RequestExecute(...).
class FakeServer {
 public:
  FakeServer(CompletionQueue *cq, std::function<TimePoint()> now) : mCq(cq), mNow(std::move(now)) {}

  // Ask to be told (via tag on the CQ) when the next request arrives.
  void requestCall(Request *out, void *tag) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mShutdown) {
      mCq->post(tag, false);
      return;
    }
    if (!mIncoming.empty()) {
      *out = mIncoming.front();
      mIncoming.pop_front();
      ++mInFlight;
      mCq->post(tag, true);
    } else {
      mWaiting.emplace_back(out, tag);
    }
  }

  // A client sends a request.
  void inject(const Request &r) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mShutdown) {
      return;  // a real client would get UNAVAILABLE
    }
    if (!mWaiting.empty()) {
      auto [out, tag] = mWaiting.front();
      mWaiting.pop_front();
      *out = r;
      ++mInFlight;
      mCq->post(tag, true);
    } else {
      mIncoming.push_back(r);
    }
  }

  // Like responder.Finish(response, status, tag): send, then tell the CQ.
  void finish(const Response &resp, void *tag) {
    std::lock_guard<std::mutex> lock(mMutex);
    mResponses.push_back(resp);
    mCq->post(tag, true);
    --mInFlight;
    mIdle.notify_all();
  }

  // Like server->Shutdown() followed by cq->Shutdown(): pending requestCalls
  // complete with ok=false, in-flight calls are allowed to finish (gRPC's
  // Shutdown() waits for them too), and only then is the CQ shut down, so
  // no operation can post to a closed queue. The CQ loop must keep running
  // while this waits, or it deadlocks.
  void shutdown() {
    std::unique_lock<std::mutex> lock(mMutex);
    mShutdown = true;
    for (auto &[out, tag] : mWaiting) {
      (void)out;
      mCq->post(tag, false);
    }
    mWaiting.clear();
    if (!mIdle.wait_for(lock, std::chrono::seconds(5), [this] { return mInFlight == 0; })) {
      mCq->shutdown();  // unblock the loop so the test can finish
      throw std::runtime_error("shutdown: calls still in flight after 5s. Does PROCESS call finish(), "
                               "and is the CQ loop still running?");
    }
    mCq->shutdown();
  }

  TimePoint now() const { return mNow(); }

  std::vector<Response> responses() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mResponses;
  }

 private:
  CompletionQueue *mCq;
  std::function<TimePoint()> mNow;
  mutable std::mutex mMutex;
  std::deque<Request> mIncoming;
  std::deque<std::pair<Request *, void *>> mWaiting;
  std::vector<Response> mResponses;
  std::condition_variable mIdle;
  int mInFlight = 0;  // requests handed to a CallData but not yet finished
  bool mShutdown = false;
};

// ------------------------------------------------------------ YOUR CODE (2)
// One in-flight RPC, exactly like CallData in Gringofts' app_util/RequestCallData.h.
// Always heap-allocated; it deletes itself when finished.
class CallData {
 public:
  using Handler = std::function<int64_t(const Request &)>;

  CallData(FakeServer *server, Handler handler) : mServer(server), mHandler(std::move(handler)) {
    ++sAlive;
    proceed(true);
  }

  // Called by the CQ loop with the (tag, ok) event for this object.
  // TODO: implement the state machine (compare Gringofts' RequestCallData.h):
  //   !ok            -> delete this (the requestCall was cancelled by shutdown)
  //   CREATE         -> state = PROCESS; mServer->requestCall(&mRequest, this)
  //   PROCESS        -> new CallData(mServer, mHandler) so the next request is accepted;
  //                     build a Response: DEADLINE_EXCEEDED if mServer->now() > deadline
  //                     (don't call the handler), else OK with mHandler(mRequest);
  //                     state = FINISH; mServer->finish(resp, this)
  //   FINISH         -> delete this
  void proceed(bool ok) {
    (void)ok;
  }

  static int alive() { return sAlive.load(); }

 private:
  enum class State { CREATE, PROCESS, FINISH };
  ~CallData() { --sAlive; }  // private: only proceed() may delete

  FakeServer *mServer;
  Handler mHandler;
  Request mRequest;
  State mState = State::CREATE;
  inline static std::atomic<int> sAlive{0};
};

// The event loop a gRPC server thread runs.
inline void runLoop(CompletionQueue *cq) {
  void *tag = nullptr;
  bool ok = false;
  while (cq->next(&tag, &ok)) {
    static_cast<CallData *>(tag)->proceed(ok);
  }
}

}  // namespace j2c::rpc

#endif  // J2C_ASYNC_CQ_H_
