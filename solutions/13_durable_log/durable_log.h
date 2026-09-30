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
  Fd(const std::string &path, int flags, mode_t mode = 0644) : mFd(::open(path.c_str(), flags, mode)) {
    if (mFd < 0) {
      throw std::system_error(errno, std::generic_category(), "open " + path);
    }
  }
  ~Fd() {
    if (mFd >= 0) {
      ::close(mFd);
    }
  }
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
  Fd(Fd &&other) noexcept : mFd(std::exchange(other.mFd, -1)) {}
  Fd &operator=(Fd &&other) noexcept {
    if (this != &other) {
      if (mFd >= 0) {
        ::close(mFd);
      }
      mFd = std::exchange(other.mFd, -1);
    }
    return *this;
  }
  int get() const { return mFd; }

 private:
  int mFd = -1;
};

inline void writeFully(int fd, const char *data, size_t len) {
  while (len > 0) {
    ssize_t n = ::write(fd, data, len);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(), "write");
    }
    data += n;
    len -= static_cast<size_t>(n);
  }
}

class SegmentWriter {
 public:
  static SegmentWriter open(const std::string &path) {
    return SegmentWriter(Fd(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644));
  }

  void append(std::string_view payload) {
    // One buffer, one write() call per frame: fewer syscalls, and a crash can
    // only tear the frame at the end.
    std::string frame(8 + payload.size(), '\0');
    auto len = static_cast<uint32_t>(payload.size());
    uint32_t crc = crc32(payload.data(), payload.size());
    std::memcpy(frame.data(), &len, 4);
    std::memcpy(frame.data() + 4, &crc, 4);
    std::memcpy(frame.data() + 8, payload.data(), payload.size());
    writeFully(mFd.get(), frame.data(), frame.size());
  }

  void sync() {
    if (::fdatasync(mFd.get()) != 0) {
      throw std::system_error(errno, std::generic_category(), "fdatasync");
    }
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
  RecoveryResult result;
  if (::access(path.c_str(), F_OK) != 0 && errno == ENOENT) {
    return result;  // no segment yet
  }
  Fd fd(path, O_RDWR | O_CLOEXEC);

  std::string data;
  char buf[64 * 1024];
  while (true) {
    ssize_t n = ::read(fd.get(), buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(), "read " + path);
    }
    if (n == 0) {
      break;
    }
    data.append(buf, static_cast<size_t>(n));
  }

  size_t pos = 0;
  while (pos < data.size()) {
    if (data.size() - pos < 8) {
      result.truncatedTail = true;  // torn header
      break;
    }
    uint32_t len = 0, crc = 0;
    std::memcpy(&len, data.data() + pos, 4);
    std::memcpy(&crc, data.data() + pos + 4, 4);
    size_t end = pos + 8 + len;
    if (end > data.size()) {
      result.truncatedTail = true;  // torn payload
      break;
    }
    if (crc32(data.data() + pos + 8, len) != crc) {
      if (end == data.size()) {
        result.truncatedTail = true;  // last frame partially written
        break;
      }
      throw std::runtime_error("corrupt frame at offset " + std::to_string(pos) + " in " + path);
    }
    result.records.emplace_back(data, pos + 8, len);
    pos = end;
  }
  result.validBytes = pos;

  if (result.truncatedTail) {
    if (::ftruncate(fd.get(), static_cast<off_t>(pos)) != 0) {
      throw std::system_error(errno, std::generic_category(), "ftruncate " + path);
    }
    if (::fdatasync(fd.get()) != 0) {
      throw std::system_error(errno, std::generic_category(), "fdatasync " + path);
    }
  }
  return result;
}

}  // namespace j2c::storage

#endif  // J2C_DURABLE_LOG_H_
