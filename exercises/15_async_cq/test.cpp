#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "async_cq.h"

using namespace j2c::rpc;  // NOLINT
using namespace std::chrono_literals;  // NOLINT

namespace {

// Runs the CQ loop on a thread; exceptions are reported instead of terminating.
class LoopThread {
 public:
  explicit LoopThread(CompletionQueue *cq)
      : mThread([cq] {
          try {
            runLoop(cq);
          } catch (const std::exception &e) {
            ADD_FAILURE() << "CQ loop threw: " << e.what();
          }
        }) {}
  ~LoopThread() { mThread.join(); }

 private:
  std::thread mThread;
};

bool waitFor(const std::function<bool()> &cond, std::chrono::milliseconds timeout = 2000ms) {
  auto end = Clock::now() + timeout;
  while (Clock::now() < end) {
    if (cond()) {
      return true;
    }
    std::this_thread::sleep_for(1ms);
  }
  return cond();
}

}  // namespace

TEST(CompletionQueueTest, FifoThenFalseAfterShutdown) {
  CompletionQueue cq;
  int a = 0, b = 0;
  cq.post(&a, true);
  cq.post(&b, false);
  cq.shutdown();
  void *tag = nullptr;
  bool ok = false;
  ASSERT_TRUE(cq.next(&tag, &ok));
  EXPECT_EQ(tag, &a);
  EXPECT_TRUE(ok);
  ASSERT_TRUE(cq.next(&tag, &ok)) << "queued events must drain after shutdown";
  EXPECT_EQ(tag, &b);
  EXPECT_FALSE(ok);
  EXPECT_FALSE(cq.next(&tag, &ok));
  EXPECT_THROW(cq.post(&a, true), std::logic_error);
}

TEST(CompletionQueueTest, NextBlocksUntilPost) {
  CompletionQueue cq;
  std::atomic<bool> got{false};
  int x = 0;
  std::thread t([&] {
    try {
      void *tag = nullptr;
      bool ok = false;
      if (cq.next(&tag, &ok)) {
        got = (tag == &x);
      }
    } catch (const std::exception &e) {
      ADD_FAILURE() << e.what();
    }
  });
  std::this_thread::sleep_for(30ms);
  EXPECT_FALSE(got.load());
  cq.post(&x, true);
  t.join();
  EXPECT_TRUE(got.load());
}

TEST(AsyncServer, ServesAllRequestsAndLeaksNothing) {
  CompletionQueue cq;
  FakeServer server(&cq, [] { return Clock::now(); });
  {
    LoopThread loop(&cq);
    new CallData(&server, [](const Request &r) { return r.value * 2; });
    for (uint64_t i = 1; i <= 200; ++i) {
      server.inject(Request{i, static_cast<int64_t>(i), TimePoint::max()});
    }
    EXPECT_TRUE(waitFor([&] { return server.responses().size() == 200; }))
        << "only " << server.responses().size() << " responses";
    server.shutdown();
  }  // loop joined
  for (const auto &r : server.responses()) {
    EXPECT_EQ(r.status, StatusCode::OK);
    EXPECT_EQ(r.result, static_cast<int64_t>(r.id) * 2);
  }
  EXPECT_EQ(CallData::alive(), 0) << "every CallData must delete itself";
}

TEST(AsyncServer, ExpiredDeadlineSkipsHandler) {
  CompletionQueue cq;
  std::atomic<int> handled{0};
  TimePoint fakeNow = Clock::now();
  FakeServer server(&cq, [&fakeNow] { return fakeNow; });
  {
    LoopThread loop(&cq);
    new CallData(&server, [&handled](const Request &r) {
      ++handled;
      return r.value;
    });
    server.inject(Request{1, 10, fakeNow - 1ms});  // already expired
    server.inject(Request{2, 20, fakeNow + 1s});
    EXPECT_TRUE(waitFor([&] { return server.responses().size() == 2; }));
    server.shutdown();
  }
  auto responses = server.responses();
  ASSERT_EQ(responses.size(), 2u);
  EXPECT_EQ(responses[0].status, StatusCode::DEADLINE_EXCEEDED);
  EXPECT_EQ(responses[1].status, StatusCode::OK);
  EXPECT_EQ(responses[1].result, 20);
  EXPECT_EQ(handled.load(), 1);
  EXPECT_EQ(CallData::alive(), 0);
}

TEST(AsyncServer, ShutdownWithNoTrafficCleansUp) {
  CompletionQueue cq;
  FakeServer server(&cq, [] { return Clock::now(); });
  {
    LoopThread loop(&cq);
    new CallData(&server, [](const Request &r) { return r.value; });
    std::this_thread::sleep_for(10ms);
    server.shutdown();
  }
  EXPECT_TRUE(server.responses().empty());
  EXPECT_EQ(CallData::alive(), 0);
}
