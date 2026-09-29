#ifndef J2C_WIRE_H_
#define J2C_WIRE_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace j2c::wire {

enum WireType : uint32_t { kVarint = 0, kLengthDelimited = 2 };

inline void putVarint(std::string *out, uint64_t v) {
  while (v >= 0x80) {
    out->push_back(static_cast<char>((v & 0x7F) | 0x80));
    v >>= 7;
  }
  out->push_back(static_cast<char>(v));
}

// Reads a varint from the front of *in and advances *in past it.
inline uint64_t getVarint(std::string_view *in) {
  uint64_t result = 0;
  for (int i = 0; i < 10; ++i) {
    if (in->empty()) {
      throw std::runtime_error("truncated varint");
    }
    auto byte = static_cast<uint8_t>(in->front());
    in->remove_prefix(1);
    result |= static_cast<uint64_t>(byte & 0x7F) << (7 * i);
    if ((byte & 0x80) == 0) {
      return result;
    }
  }
  throw std::runtime_error("varint longer than 10 bytes");
}

inline uint64_t zigzagEncode(int64_t n) {
  return (static_cast<uint64_t>(n) << 1) ^ static_cast<uint64_t>(n >> 63);
}
inline int64_t zigzagDecode(uint64_t n) {
  return static_cast<int64_t>(n >> 1) ^ -static_cast<int64_t>(n & 1);
}

// ---- helpers shared by both message versions ----

inline void putTag(std::string *out, uint32_t field, WireType type) {
  putVarint(out, (static_cast<uint64_t>(field) << 3) | type);
}

inline void putString(std::string *out, uint32_t field, const std::string &s) {
  if (s.empty()) {
    return;
  }
  putTag(out, field, kLengthDelimited);
  putVarint(out, s.size());
  out->append(s);
}

inline void putUint(std::string *out, uint32_t field, uint64_t v) {
  if (v == 0) {
    return;
  }
  putTag(out, field, kVarint);
  putVarint(out, v);
}

inline std::string_view getLengthDelimited(std::string_view *in) {
  uint64_t len = getVarint(in);
  if (len > in->size()) {
    throw std::runtime_error("truncated length-delimited field");
  }
  std::string_view value = in->substr(0, len);
  in->remove_prefix(len);
  return value;
}

inline void expectType(uint32_t field, uint32_t actual, WireType expected) {
  if (actual != expected) {
    throw std::runtime_error("field " + std::to_string(field) + " has wire type " + std::to_string(actual) +
                             ", expected " + std::to_string(expected));
  }
}

// Skips one field value and returns the raw bytes (tag included) that were consumed.
inline std::string_view skipField(std::string_view fieldStart, std::string_view *in, uint32_t type) {
  switch (type) {
    case kVarint:
      getVarint(in);
      break;
    case kLengthDelimited:
      getLengthDelimited(in);
      break;
    case 1:  // fixed64
    case 5:  // fixed32
    {
      size_t n = type == 1 ? 8 : 4;
      if (in->size() < n) {
        throw std::runtime_error("truncated fixed field");
      }
      in->remove_prefix(n);
      break;
    }
    default:
      throw std::runtime_error("unsupported wire type " + std::to_string(type));
  }
  return fieldStart.substr(0, fieldStart.size() - in->size());
}

// ---- v1 ----

struct RequestV1 {
  uint64_t value = 0;
  std::string requestId;
  std::string unknownFields;  // raw bytes of fields this version doesn't know
};

inline std::string encode(const RequestV1 &r) {
  std::string out;
  putUint(&out, 1, r.value);
  putString(&out, 2, r.requestId);
  out.append(r.unknownFields);
  return out;
}

inline RequestV1 decode(std::string_view in) {
  RequestV1 r;
  while (!in.empty()) {
    std::string_view fieldStart = in;
    uint64_t tag = getVarint(&in);
    auto field = static_cast<uint32_t>(tag >> 3);
    auto type = static_cast<uint32_t>(tag & 7);
    switch (field) {
      case 1:
        expectType(field, type, kVarint);
        r.value = getVarint(&in);
        break;
      case 2:
        expectType(field, type, kLengthDelimited);
        r.requestId = std::string(getLengthDelimited(&in));
        break;
      default:
        r.unknownFields.append(skipField(fieldStart, &in, type));
    }
  }
  return r;
}

// ---- v2 ----

struct RequestV2 {
  uint64_t value = 0;
  std::string requestId;
  int64_t delta = 0;
  std::string trackingContext;
};

inline std::string encode(const RequestV2 &r) {
  std::string out;
  putUint(&out, 1, r.value);
  putString(&out, 2, r.requestId);
  putUint(&out, 3, zigzagEncode(r.delta));
  putString(&out, 4, r.trackingContext);
  return out;
}

inline RequestV2 decodeV2(std::string_view in) {
  RequestV2 r;
  while (!in.empty()) {
    std::string_view fieldStart = in;
    uint64_t tag = getVarint(&in);
    auto field = static_cast<uint32_t>(tag >> 3);
    auto type = static_cast<uint32_t>(tag & 7);
    switch (field) {
      case 1:
        expectType(field, type, kVarint);
        r.value = getVarint(&in);
        break;
      case 2:
        expectType(field, type, kLengthDelimited);
        r.requestId = std::string(getLengthDelimited(&in));
        break;
      case 3:
        expectType(field, type, kVarint);
        r.delta = zigzagDecode(getVarint(&in));
        break;
      case 4:
        expectType(field, type, kLengthDelimited);
        r.trackingContext = std::string(getLengthDelimited(&in));
        break;
      default:
        skipField(fieldStart, &in, type);  // v3 fields: ignored here
    }
  }
  return r;
}

}  // namespace j2c::wire

#endif  // J2C_WIRE_H_
