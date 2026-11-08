#ifndef J2C_EPOLL_SERVER_H_
#define J2C_EPOLL_SERVER_H_

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>

namespace j2c::net {

inline void throwErrno(const char *what) { throw std::system_error(errno, std::generic_category(), what); }

inline void setNonBlocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    throwErrno("fcntl O_NONBLOCK");
  }
}

// A single-threaded, level-triggered epoll server for a newline-framed
// request/response protocol: every complete line "xyz\n" gets handler("xyz")
// + "\n" back. This is what gRPC's completion-queue threads do underneath.
class EpollServer {
 public:
  using Handler = std::function<std::string(const std::string &)>;

  explicit EpollServer(Handler handler) : mHandler(std::move(handler)) {}
  ~EpollServer() { stop(); }
  EpollServer(const EpollServer &) = delete;
  EpollServer &operator=(const EpollServer &) = delete;

  // Binds 127.0.0.1 on an ephemeral port, starts the event-loop thread, and
  // returns the port.
  uint16_t start() {
    mListenFd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (mListenFd < 0) {
      throwErrno("socket");
    }
    int one = 1;
    ::setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // let the kernel pick
    if (::bind(mListenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
      throwErrno("bind");
    }
    if (::listen(mListenFd, SOMAXCONN) < 0) {
      throwErrno("listen");
    }
    setNonBlocking(mListenFd);
    socklen_t len = sizeof(addr);
    ::getsockname(mListenFd, reinterpret_cast<sockaddr *>(&addr), &len);

    mEpollFd = ::epoll_create1(EPOLL_CLOEXEC);
    mWakeFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (mEpollFd < 0 || mWakeFd < 0) {
      throwErrno("epoll_create1/eventfd");
    }
    watch(mListenFd, EPOLLIN);
    watch(mWakeFd, EPOLLIN);

    mThread = std::thread([this] { loop(); });
    return ntohs(addr.sin_port);
  }

  // Wakes the loop through the eventfd, joins it, closes every fd.
  void stop() {
    if (mThread.joinable()) {
      uint64_t one = 1;
      ssize_t unused = ::write(mWakeFd, &one, sizeof(one));
      (void)unused;
      mThread.join();
    }
    for (auto &[fd, conn] : mConns) {
      (void)conn;
      ::close(fd);
    }
    mConns.clear();
    for (int *fd : {&mListenFd, &mEpollFd, &mWakeFd}) {
      if (*fd >= 0) {
        ::close(*fd);
        *fd = -1;
      }
    }
  }

  size_t openConnections() const { return mOpenConnections.load(); }

 private:
  struct Conn {
    std::string in;   // bytes received, not yet a complete line
    std::string out;  // bytes to send, not yet accepted by the kernel
  };

  void watch(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    if (::epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
      throwErrno("epoll_ctl ADD");
    }
  }
  void rewatch(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    ::epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &ev);
  }

  void loop() {
    epoll_event events[64];
    while (true) {
      int n = ::epoll_wait(mEpollFd, events, 64, -1);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        return;
      }
      for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        if (fd == mWakeFd) {
          return;  // stop() asked us to exit
        }
        if (fd == mListenFd) {
          acceptAll();
          continue;
        }
        uint32_t ev = events[i].events;
        if (ev & (EPOLLHUP | EPOLLERR)) {
          closeConn(fd);
          continue;
        }
        if ((ev & EPOLLIN) && !onReadable(fd)) {
          continue;  // connection closed
        }
        if (ev & EPOLLOUT) {
          onWritable(fd);
        }
      }
    }
  }

  void acceptAll() {
    while (true) {  // level-triggered, but draining the backlog saves wakeups
      int fd = ::accept4(mListenFd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
      if (fd < 0) {
        return;  // EAGAIN: no more pending connections (or a transient error)
      }
      mConns.emplace(fd, Conn{});
      ++mOpenConnections;
      watch(fd, EPOLLIN | EPOLLRDHUP);
    }
  }

  // Returns false if the connection was closed.
  bool onReadable(int fd) {
    auto it = mConns.find(fd);
    if (it == mConns.end()) {
      return false;  // stale event for an fd closed earlier in this batch
    }
    Conn &c = it->second;
    char buf[64 * 1024];
    while (true) {
      ssize_t r = ::read(fd, buf, sizeof(buf));
      if (r > 0) {
        c.in.append(buf, static_cast<size_t>(r));
        continue;
      }
      if (r == 0) {  // peer closed
        closeConn(fd);
        return false;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // drained for now
      }
      closeConn(fd);
      return false;
    }
    // Frame: handle every complete line; keep a partial tail for later.
    size_t start = 0;
    for (size_t nl; (nl = c.in.find('\n', start)) != std::string::npos; start = nl + 1) {
      c.out += mHandler(c.in.substr(start, nl - start));
      c.out += '\n';
    }
    c.in.erase(0, start);
    return flush(fd, c);
  }

  void onWritable(int fd) {
    auto it = mConns.find(fd);
    if (it != mConns.end()) {
      flush(fd, it->second);
    }
  }

  // Writes as much of c.out as the kernel accepts. If some is left, ask
  // epoll for EPOLLOUT; when everything is sent, stop asking (otherwise
  // epoll reports "writable" forever and the loop spins at 100% CPU).
  bool flush(int fd, Conn &c) {
    while (!c.out.empty()) {
      ssize_t w = ::send(fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);  // no SIGPIPE
      if (w > 0) {
        c.out.erase(0, static_cast<size_t>(w));
        continue;
      }
      if (w < 0 && errno == EINTR) {
        continue;
      }
      if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        rewatch(fd, EPOLLIN | EPOLLRDHUP | EPOLLOUT);
        return true;
      }
      closeConn(fd);
      return false;
    }
    rewatch(fd, EPOLLIN | EPOLLRDHUP);
    return true;
  }

  void closeConn(int fd) {
    ::epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    if (mConns.erase(fd) != 0) {
      --mOpenConnections;
    }
  }

  Handler mHandler;
  int mListenFd = -1;
  int mEpollFd = -1;
  int mWakeFd = -1;
  std::unordered_map<int, Conn> mConns;  // touched only by the loop thread (and stop() after join)
  std::atomic<size_t> mOpenConnections{0};
  std::thread mThread;
};

}  // namespace j2c::net

#endif  // J2C_EPOLL_SERVER_H_
