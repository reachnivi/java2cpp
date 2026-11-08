#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <memory>
#include <chrono>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "epoll_server.h"

using j2c::net::EpollServer;
using namespace std::chrono_literals;  // NOLINT

namespace {

std::string upper(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::toupper(c); });
  return out;
}

// A plain BLOCKING client socket, with a receive timeout so a broken server
// makes the test fail instead of hang.
class Client {
 public:
  explicit Client(uint16_t port) {
    mFd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    timeval tv{5, 0};
    ::setsockopt(mFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    mConnected = ::connect(mFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
  }
  ~Client() { close(); }
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  bool connected() const { return mConnected; }

  bool sendAll(const std::string &data) {
    size_t sent = 0;
    while (sent < data.size()) {
      ssize_t w = ::send(mFd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
      if (w <= 0) {
        return false;
      }
      sent += static_cast<size_t>(w);
    }
    return true;
  }

  // Reads until `lines` newlines arrive, EOF, or the 5 s timeout.
  std::string readLines(int lines) {
    std::string got;
    char buf[64 * 1024];
    while (std::count(got.begin(), got.end(), '\n') < lines) {
      ssize_t r = ::recv(mFd, buf, sizeof(buf), 0);
      if (r <= 0) {
        break;
      }
      got.append(buf, static_cast<size_t>(r));
    }
    return got;
  }

  // True if the server closed the connection (recv returns 0).
  bool seesEof() {
    char c;
    return ::recv(mFd, &c, 1, 0) == 0;
  }

  void close() {
    if (mFd >= 0) {
      ::close(mFd);
      mFd = -1;
    }
  }

 private:
  int mFd = -1;
  bool mConnected = false;
};

// Starts the server, or skips the test if this machine has no loopback networking.
#define START_OR_SKIP(server, port)                                     \
  uint16_t port = 0;                                                    \
  try {                                                                 \
    port = (server).start();                                            \
  } catch (const std::system_error &e) {                                \
    GTEST_SKIP() << "no loopback sockets here: " << e.what();           \
  }

bool eventually(const std::function<bool()> &cond, std::chrono::milliseconds timeout = 3000ms) {
  auto end = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < end) {
    if (cond()) {
      return true;
    }
    std::this_thread::sleep_for(5ms);
  }
  return cond();
}

}  // namespace

TEST(EpollServerTest, RequestResponse) {
  EpollServer server(upper);
  START_OR_SKIP(server, port);
  Client c(port);
  ASSERT_TRUE(c.connected());
  ASSERT_TRUE(c.sendAll("hello\nworld\n"));
  EXPECT_EQ(c.readLines(2), "HELLO\nWORLD\n");
}

TEST(EpollServerTest, MessageSplitAcrossReads) {
  EpollServer server(upper);
  START_OR_SKIP(server, port);
  Client c(port);
  ASSERT_TRUE(c.connected());
  ASSERT_TRUE(c.sendAll("ra"));
  std::this_thread::sleep_for(30ms);  // make sure the server sees a partial line first
  ASSERT_TRUE(c.sendAll("ft\nlo"));
  std::this_thread::sleep_for(30ms);
  ASSERT_TRUE(c.sendAll("g\n"));
  EXPECT_EQ(c.readLines(2), "RAFT\nLOG\n");
}

TEST(EpollServerTest, ManyConcurrentClients) {
  EpollServer server([](const std::string &s) { return "ok:" + s; });
  START_OR_SKIP(server, port);
  constexpr int kClients = 50;
  constexpr int kRequests = 20;
  std::atomic<int> good{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < kClients; ++t) {
    threads.emplace_back([&, t] {
      Client c(port);
      if (!c.connected()) {
        return;
      }
      for (int i = 0; i < kRequests; ++i) {
        std::string req = std::to_string(t) + "-" + std::to_string(i);
        if (!c.sendAll(req + "\n") || c.readLines(1) != "ok:" + req + "\n") {
          return;
        }
      }
      ++good;
    });
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(good.load(), kClients) << "every client must get every response, in order";
}

TEST(EpollServerTest, LargeMessageForcesPartialWrites) {
  EpollServer server([](const std::string &s) { return s; });
  START_OR_SKIP(server, port);
  Client c(port);
  ASSERT_TRUE(c.connected());
  std::string big(1 << 20, 'x');  // 1 MB: far bigger than a socket buffer
  // Send from another thread: the server starts answering before we finish.
  std::thread sender([&] { c.sendAll(big + "\n"); });
  std::string reply = c.readLines(1);
  sender.join();
  EXPECT_EQ(reply.size(), big.size() + 1) << "the server must buffer what send() didn't accept and retry on EPOLLOUT";
}

TEST(EpollServerTest, ClientDisconnectMidMessageIsCleanedUp) {
  EpollServer server(upper);
  START_OR_SKIP(server, port);
  {
    Client c(port);
    ASSERT_TRUE(c.connected());
    ASSERT_TRUE(eventually([&] { return server.openConnections() == 1; }));
    c.sendAll("half a mess");  // no newline, then disconnect
  }
  EXPECT_TRUE(eventually([&] { return server.openConnections() == 0; }))
      << "the server must notice EOF and close its side";
  Client again(port);  // and it still serves new clients
  ASSERT_TRUE(again.connected());
  ASSERT_TRUE(again.sendAll("still up\n"));
  EXPECT_EQ(again.readLines(1), "STILL UP\n");
}

TEST(EpollServerTest, StopIsPromptAndClosesConnections) {
  auto server = std::make_unique<EpollServer>(upper);
  START_OR_SKIP(*server, port);
  Client idle(port);
  ASSERT_TRUE(idle.connected());
  ASSERT_TRUE(eventually([&] { return server->openConnections() == 1; }));
  auto start = std::chrono::steady_clock::now();
  server->stop();  // the loop is blocked in epoll_wait(-1): only the eventfd can wake it
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
  EXPECT_TRUE(idle.seesEof()) << "stop() must close client connections";
  server.reset();  // destructor after stop() must be harmless
}
