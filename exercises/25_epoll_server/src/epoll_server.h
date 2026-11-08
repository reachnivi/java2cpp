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

  // TODO: the event loop. epoll_wait(mEpollFd, events, 64, -1) (retry on
  // EINTR), then for each ready fd:
  //   mWakeFd   -> stop() asked us to exit: return
  //   mListenFd -> acceptAll()
  //   a client  -> EPOLLHUP/EPOLLERR: closeConn; EPOLLIN: onReadable
  //                (returns false if it closed the conn); EPOLLOUT: onWritable
  void loop() {}

  // TODO: accept4(..., SOCK_NONBLOCK | SOCK_CLOEXEC) until it fails with
  // EAGAIN; add a Conn, ++mOpenConnections, watch(fd, EPOLLIN | EPOLLRDHUP).
  void acceptAll() {}

  // TODO: read() into a buffer until EAGAIN (non-blocking!); r == 0 means the
  // peer closed -> closeConn, return false. Then frame: for every complete
  // line in c.in, append mHandler(line) + "\n" to c.out; keep any partial tail
  // in c.in. Finally flush(). Guard against a stale fd (mConns.find).
  bool onReadable(int fd) {
    (void)fd;
    return false;
  }

  void onWritable(int fd) {
    auto it = mConns.find(fd);
    if (it != mConns.end()) {
      flush(fd, it->second);
    }
  }

  // TODO: send(fd, ..., MSG_NOSIGNAL) as much of c.out as the kernel takes.
  // EAGAIN with data left -> rewatch with EPOLLOUT added. Everything sent ->
  // rewatch WITHOUT EPOLLOUT (or epoll says "writable" forever: 100% CPU).
  // Any other error -> closeConn, return false.
  bool flush(int fd, Conn &c) {
    (void)fd;
    (void)c;
    return true;
  }

  // TODO: EPOLL_CTL_DEL, close, erase from mConns, --mOpenConnections.
  void closeConn(int fd) { (void)fd; }

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
