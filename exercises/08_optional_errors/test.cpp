#include <gtest/gtest.h>

#include "config.h"

using namespace j2c;  // NOLINT

TEST(Optional, ParsePort) {
  EXPECT_EQ(parsePort("50051"), 50051);
  EXPECT_EQ(parsePort("1"), 1);
  EXPECT_EQ(parsePort("65535"), 65535);
  EXPECT_EQ(parsePort("0"), std::nullopt);
  EXPECT_EQ(parsePort("65536"), std::nullopt);
  EXPECT_EQ(parsePort("80a"), std::nullopt);
  EXPECT_EQ(parsePort(""), std::nullopt);
  EXPECT_EQ(parsePort("-1"), std::nullopt);
  EXPECT_EQ(parsePort("99999999999999999999"), std::nullopt);
}

static const char *kIni = R"(
; Gringofts-style config
[raft]
cluster.conf = 1@10.0.0.1:5254,2@10.0.0.2:5254
storage.type=file

# another comment
[grpc]
  port   =   50051
bad.port = fifty
)";

TEST(Config, GetReturnsTrimmedValues) {
  auto cfg = Config::parse(kIni);
  EXPECT_EQ(cfg.get("raft", "cluster.conf"), "1@10.0.0.1:5254,2@10.0.0.2:5254");
  EXPECT_EQ(cfg.get("raft", "storage.type"), "file");
  EXPECT_EQ(cfg.get("grpc", "port"), "50051");
}

TEST(Config, MissingIsNullopt) {
  auto cfg = Config::parse(kIni);
  EXPECT_EQ(cfg.get("raft", "nope"), std::nullopt);
  EXPECT_EQ(cfg.get("nosection", "port"), std::nullopt);
  EXPECT_EQ(cfg.get("raft", "port"), std::nullopt);  // keys are per-section
}

TEST(Config, GetOr) {
  auto cfg = Config::parse(kIni);
  EXPECT_EQ(cfg.getOr("raft", "storage.type", "rocksdb"), "file");
  EXPECT_EQ(cfg.getOr("raft", "missing", "rocksdb"), "rocksdb");
}

TEST(Config, GetIntThrowsDistinctExceptions) {
  auto cfg = Config::parse(kIni);
  EXPECT_EQ(cfg.getInt("grpc", "port"), 50051);
  EXPECT_THROW(cfg.getInt("grpc", "missing"), std::out_of_range);
  EXPECT_THROW(cfg.getInt("grpc", "bad.port"), std::invalid_argument);
}

TEST(Config, MalformedLineThrows) {
  EXPECT_THROW(Config::parse("[raft]\nthis line has no equals\n"), std::invalid_argument);
  EXPECT_THROW(Config::parse("[unterminated\n"), std::invalid_argument);
}

TEST(ProcessHintTest, MirrorsGringoftsIncreaseHandler) {
  auto ok = validateIncrease(5, 6);
  EXPECT_EQ(ok.mCode, 200u);
  EXPECT_EQ(ok.mMessage, "Success");
  EXPECT_EQ(validateIncrease(5, 5).mCode, 201u);
  EXPECT_EQ(validateIncrease(5, 3).mCode, 201u);
  EXPECT_EQ(validateIncrease(5, 7).mCode, 400u);
}
