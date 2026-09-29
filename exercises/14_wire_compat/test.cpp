#include <gtest/gtest.h>

#include <limits>
#include <string>

#include "wire.h"

using namespace j2c::wire;  // NOLINT

static std::string bytes(std::initializer_list<int> b) {
  std::string s;
  for (int x : b) {
    s.push_back(static_cast<char>(x));
  }
  return s;
}

TEST(Varint, EncodesLikeProtobuf) {
  std::string out;
  putVarint(&out, 1);
  EXPECT_EQ(out, bytes({0x01}));
  out.clear();
  putVarint(&out, 300);
  EXPECT_EQ(out, bytes({0xAC, 0x02}));
  out.clear();
  putVarint(&out, 0);
  EXPECT_EQ(out, bytes({0x00}));
}

TEST(Varint, RoundTripsExtremes) {
  for (uint64_t v : {uint64_t{0}, uint64_t{127}, uint64_t{128}, uint64_t{1} << 35,
                     std::numeric_limits<uint64_t>::max()}) {
    std::string buf;
    putVarint(&buf, v);
    std::string_view in(buf);
    EXPECT_EQ(getVarint(&in), v);
    EXPECT_TRUE(in.empty()) << "getVarint must consume exactly the varint";
  }
  std::string max;
  putVarint(&max, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(max.size(), 10u);
}

TEST(Varint, TruncatedInputThrows) {
  std::string_view in("\x80\x80", 2);  // "more bytes follow" but nothing does
  EXPECT_THROW(getVarint(&in), std::runtime_error);
}

TEST(ZigZag, SmallNegativesStaySmall) {
  EXPECT_EQ(zigzagEncode(0), 0u);
  EXPECT_EQ(zigzagEncode(-1), 1u);
  EXPECT_EQ(zigzagEncode(1), 2u);
  EXPECT_EQ(zigzagEncode(-2), 3u);
  for (int64_t v : {int64_t{0}, int64_t{-5}, int64_t{12345}, std::numeric_limits<int64_t>::min(),
                    std::numeric_limits<int64_t>::max()}) {
    EXPECT_EQ(zigzagDecode(zigzagEncode(v)), v);
  }
}

TEST(Messages, V1EncodingMatchesProtobuf) {
  // What protoc-generated code would produce for {value: 150, request_id: "ab"}
  RequestV1 r{150, "ab", ""};
  EXPECT_EQ(encode(r), bytes({0x08, 0x96, 0x01, 0x12, 0x02, 'a', 'b'}));
}

TEST(Messages, DefaultsAreOmitted) {
  EXPECT_EQ(encode(RequestV1{}), "");
  EXPECT_EQ(encode(RequestV2{}), "");
}

TEST(Messages, V2RoundTrip) {
  RequestV2 r{7, "req-1", -3, "trace=abc"};
  RequestV2 back = decodeV2(encode(r));
  EXPECT_EQ(back.value, 7u);
  EXPECT_EQ(back.requestId, "req-1");
  EXPECT_EQ(back.delta, -3);
  EXPECT_EQ(back.trackingContext, "trace=abc");
}

TEST(Compat, NewReaderOldData) {
  // Upgrade: new binary replays entries an old binary wrote.
  std::string old = encode(RequestV1{5, "r", ""});
  RequestV2 r = decodeV2(old);
  EXPECT_EQ(r.value, 5u);
  EXPECT_EQ(r.requestId, "r");
  EXPECT_EQ(r.delta, 0) << "missing field must read as its default";
  EXPECT_EQ(r.trackingContext, "");
}

TEST(Compat, OldReaderNewData) {
  // Rollback: old binary replays entries a new binary wrote.
  std::string fresh = encode(RequestV2{9, "r2", -4, "ctx"});
  RequestV1 r = decode(fresh);
  EXPECT_EQ(r.value, 9u);
  EXPECT_EQ(r.requestId, "r2");
  EXPECT_FALSE(r.unknownFields.empty()) << "unknown fields must be kept, not dropped";
}

TEST(Compat, OldNodeForwardsNewMessageWithoutLoss) {
  // An old follower decodes, then re-encodes to forward to the leader.
  std::string fresh = encode(RequestV2{9, "r2", -4, "ctx"});
  std::string forwarded = encode(decode(fresh));
  RequestV2 atLeader = decodeV2(forwarded);
  EXPECT_EQ(atLeader.delta, -4);
  EXPECT_EQ(atLeader.trackingContext, "ctx");
}

TEST(Compat, ReusedFieldNumberWithWrongTypeIsRejected) {
  // Someone "reused" field 1 as a string in a later version.
  std::string evil = bytes({0x0A, 0x01, 'x'});  // field 1, wire type 2
  EXPECT_THROW(decode(evil), std::runtime_error);
}

TEST(Compat, TruncatedMessageThrows) {
  std::string full = encode(RequestV2{9, "request-id", -4, "ctx"});
  EXPECT_THROW(decodeV2(full.substr(0, full.size() - 2)), std::runtime_error);
}
