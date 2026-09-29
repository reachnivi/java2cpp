#ifndef J2C_WIRE_H_
#define J2C_WIRE_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace j2c::wire {

enum WireType : uint32_t { kVarint = 0, kLengthDelimited = 2 };

// TODO: 7 bits per byte, low groups first, set 0x80 on every byte except the last.
inline void putVarint(std::string *out, uint64_t v) {
  (void)out;
  (void)v;
}

// TODO: read a varint from the front of *in and advance *in past it
// (in->remove_prefix(n)). Throw std::runtime_error if the input ends while
// the continuation bit is set, or after 10 bytes.
inline uint64_t getVarint(std::string_view *in) {
  (void)in;
  throw std::logic_error("TODO: getVarint");
}

// TODO: (n << 1) ^ (n >> 63), computed in uint64_t
inline uint64_t zigzagEncode(int64_t n) {
  (void)n;
  return 0;
}
// TODO: inverse of zigzagEncode
inline int64_t zigzagDecode(uint64_t n) {
  (void)n;
  return 0;
}

// ---- helpers shared by both message versions (given; they use your varint code) ----

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
  // TODO: putUint field 1, putString field 2, then append unknownFields verbatim
  (void)r;
  return "TODO";
}

inline RequestV1 decode(std::string_view in) {
  // TODO: loop: remember fieldStart = in; read tag; field = tag >> 3, type = tag & 7;
  //   known fields: expectType(...) then read the value;
  //   unknown: r.unknownFields.append(skipField(fieldStart, &in, type));
  (void)in;
  throw std::logic_error("TODO: decode v1");
}

// ---- v2 ----

struct RequestV2 {
  uint64_t value = 0;
  std::string requestId;
  int64_t delta = 0;
  std::string trackingContext;
};

inline std::string encode(const RequestV2 &r) {
  // TODO: like v1 plus field 3 (zigzag varint) and field 4 (string)
  (void)r;
  return "TODO";
}

inline RequestV2 decodeV2(std::string_view in) {
  // TODO: like decode(), for fields 1-4; skip (and drop) anything else
  (void)in;
  throw std::logic_error("TODO: decode v2");
}

}  // namespace j2c::wire

#endif  // J2C_WIRE_H_
