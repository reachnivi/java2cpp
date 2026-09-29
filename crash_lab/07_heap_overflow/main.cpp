// Lab 07: the process dies inside malloc/free with a glibc message such as
// "free(): invalid next size" or "malloc(): corrupted top size".
// The backtrace points at innocent code. The bug is somewhere else.
//
// History: v2 of the wire format added `createdTimeInNanos` and `crc` to the
// frame header.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "lablog.h"

struct FrameHeader {
  uint32_t type;
  uint32_t length;
  uint64_t createdTimeInNanos;  // added in v2
  uint64_t crc;                 // added in v2
};

constexpr size_t kHeaderSize = 8;  // sizeof(type) + sizeof(length)

struct Frame {
  char *data;
  size_t size;
};

Frame encode(uint32_t type, const std::string &payload) {
  FrameHeader header{type, static_cast<uint32_t>(payload.size()), 1234567890ULL, 0xC0FFEEULL};
  Frame f{new char[kHeaderSize + payload.size()], sizeof(header) + payload.size()};
  std::memcpy(f.data, &header, sizeof(header));
  std::memcpy(f.data + sizeof(header), payload.data(), payload.size());
  return f;
}

int main() {
  std::vector<std::string> sent;
  for (int i = 0; i < 1000; ++i) {
    Frame f = encode(1, "increase:" + std::to_string(i));
    sent.push_back("frame-" + std::to_string(i));
    delete[] f.data;
  }
  LOG_INFO("sent %zu frames", sent.size());
}
