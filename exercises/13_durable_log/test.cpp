#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>
#include <type_traits>

#include "durable_log.h"

using namespace j2c::storage;  // NOLINT

namespace {

class DurableLogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    mPath = ::testing::TempDir() + "j2c_log_" + info->name() + ".seg";
    std::remove(mPath.c_str());
  }
  void TearDown() override { std::remove(mPath.c_str()); }

  std::string readFile() const {
    std::ifstream in(mPath, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }
  void writeFile(const std::string &bytes) const {
    std::ofstream out(mPath, std::ios::binary | std::ios::trunc);
    out << bytes;
  }

  std::string mPath;
};

}  // namespace

TEST(Crc32, KnownValue) {
  // Standard CRC-32 test vector.
  EXPECT_EQ(crc32("123456789", 9), 0xCBF43926u);
}

TEST(FdTest, IsMoveOnlyAndReportsErrno) {
  EXPECT_FALSE(std::is_copy_constructible_v<Fd>);
  EXPECT_TRUE(std::is_move_constructible_v<Fd>);
  try {
    Fd fd("/no/such/dir/file", O_RDONLY);
    FAIL() << "expected std::system_error";
  } catch (const std::system_error &e) {
    EXPECT_EQ(e.code().value(), ENOENT);
  }
}

TEST_F(DurableLogTest, AppendThenRecover) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("increase:1");
    w.append("");  // empty payloads are legal
    w.append(std::string(100000, 'x'));
    w.sync();
  }
  auto r = recover(mPath);
  ASSERT_EQ(r.records.size(), 3u);
  EXPECT_EQ(r.records[0], "increase:1");
  EXPECT_EQ(r.records[1], "");
  EXPECT_EQ(r.records[2].size(), 100000u);
  EXPECT_FALSE(r.truncatedTail);
  EXPECT_EQ(r.validBytes, readFile().size());
}

TEST_F(DurableLogTest, FrameLayoutIsLengthCrcPayload) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("abc");
  }
  std::string bytes = readFile();
  ASSERT_EQ(bytes.size(), 8u + 3u);
  uint32_t len = 0, crc = 0;
  std::memcpy(&len, bytes.data(), 4);
  std::memcpy(&crc, bytes.data() + 4, 4);
  EXPECT_EQ(len, 3u);
  EXPECT_EQ(crc, crc32("abc", 3));
  EXPECT_EQ(bytes.substr(8), "abc");
}

TEST_F(DurableLogTest, MissingFileRecoversEmpty) {
  auto r = recover(mPath);
  EXPECT_TRUE(r.records.empty());
  EXPECT_EQ(r.validBytes, 0u);
}

TEST_F(DurableLogTest, TornTailIsTruncated) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("one");
    w.append("two");
    w.append("three-is-long");
  }
  std::string full = readFile();
  // Simulate a crash half-way through writing the last frame.
  writeFile(full.substr(0, full.size() - 5));

  auto r = recover(mPath);
  ASSERT_EQ(r.records.size(), 2u);
  EXPECT_EQ(r.records[1], "two");
  EXPECT_TRUE(r.truncatedTail);
  EXPECT_EQ(readFile().size(), r.validBytes) << "recover() must truncate the torn tail";

  // The log is usable again after recovery.
  {
    auto w = SegmentWriter::open(mPath);
    w.append("three");
  }
  auto again = recover(mPath);
  ASSERT_EQ(again.records.size(), 3u);
  EXPECT_EQ(again.records[2], "three");
  EXPECT_FALSE(again.truncatedTail);
}

TEST_F(DurableLogTest, TornHeaderIsTruncated) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("one");
  }
  std::string full = readFile();
  writeFile(full + std::string("\x05\x00", 2));  // 2 bytes of a 4-byte length field
  auto r = recover(mPath);
  ASSERT_EQ(r.records.size(), 1u);
  EXPECT_TRUE(r.truncatedTail);
  EXPECT_EQ(readFile(), full);
}

TEST_F(DurableLogTest, BadCrcOnLastFrameIsTornTail) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("one");
    w.append("two");
  }
  std::string bytes = readFile();
  bytes.back() ^= 0x01;  // flip a bit in the last payload
  writeFile(bytes);
  auto r = recover(mPath);
  ASSERT_EQ(r.records.size(), 1u);
  EXPECT_TRUE(r.truncatedTail);
}

TEST_F(DurableLogTest, CorruptionInTheMiddleIsFatal) {
  {
    auto w = SegmentWriter::open(mPath);
    w.append("one");
    w.append("two");
    w.append("three");
  }
  std::string bytes = readFile();
  bytes[8 + 3 + 8] ^= 0x01;  // first payload byte of "two"
  writeFile(bytes);
  EXPECT_THROW(recover(mPath), std::runtime_error);
  EXPECT_EQ(readFile(), bytes) << "never truncate on mid-log corruption";
}
