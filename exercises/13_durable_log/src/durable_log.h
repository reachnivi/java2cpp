#ifndef J2C_DURABLE_LOG_H_
#define J2C_DURABLE_LOG_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace j2c::storage {

// Standard CRC-32 (IEEE 802.3), table-driven. Provided.
inline uint32_t crc32(const void *data, size_t len) {
  static const std::array<uint32_t, 256> kTable = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      }
      t[i] = c;
    }
    return t;
  }();
  uint32_t crc = 0xFFFFFFFFu;
  const auto *p = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < len; ++i) {
    crc = kTable[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

class Fd {
 public:
  Fd(const std::string &path, int flags, mode_t mode = 0644) {
    // TODO: mFd = ::open(...); on failure throw
    //   std::system_error(errno, std::generic_category(), "open " + path)
    (void)path;
    (void)flags;
    (void)mode;
  }
  ~Fd() {
    // TODO: close if valid
  }
  // TODO: delete copy ctor/assignment; implement noexcept move ctor/assignment
  // (moved-from Fd holds -1). Hint: std::exchange.
  int get() const { return mFd; }

 private:
  int mFd = -1;
};

// TODO: loop until all `len` bytes are written. write() may return fewer
// bytes than asked, or -1 with errno == EINTR (just retry).
inline void writeFully(int fd, const char *data, size_t len) {
  (void)fd;
  (void)data;
  (void)len;
  throw std::logic_error("TODO: writeFully");
}

class SegmentWriter {
 public:
  static SegmentWriter open(const std::string &path) {
    // TODO: O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, mode 0644
    return SegmentWriter(Fd(path, O_RDONLY));
  }

  void append(std::string_view payload) {
    // TODO: build [len][crc][payload] in one buffer and writeFully() it.
    (void)payload;
    throw std::logic_error("TODO: append");
  }

  void sync() {
    // TODO: ::fdatasync; throw std::system_error on failure
  }

 private:
  explicit SegmentWriter(Fd fd) : mFd(std::move(fd)) {}
  Fd mFd;
};

struct RecoveryResult {
  std::vector<std::string> records;
  uint64_t validBytes = 0;
  bool truncatedTail = false;
};

inline RecoveryResult recover(const std::string &path) {
  // TODO (see README):
  //  1. missing file -> empty result
  //  2. read the whole file (read() loop, handle EINTR)
  //  3. parse frames; torn tail -> ftruncate(validBytes) + fdatasync, truncatedTail = true
  //  4. bad CRC with more data after it -> throw std::runtime_error
  (void)path;
  throw std::logic_error("TODO: recover");
}

}  // namespace j2c::storage

#endif  // J2C_DURABLE_LOG_H_
