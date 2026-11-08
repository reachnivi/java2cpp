#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "variant_events.h"

using namespace j2c::v;  // NOLINT

namespace {
// Processes a command and applies its events, like ex 12's App::handle.
ProcessHint handle(StateMachine *sm, const Command &cmd, std::vector<std::string> *log) {
  std::vector<Event> events;
  ProcessHint hint = sm->process(cmd, &events);
  for (const auto &e : events) {
    log->push_back(encode(e));
    sm->apply(e);
  }
  return hint;
}
}  // namespace

TEST(VariantEvents, IncreaseSemanticsMatchCapstone) {
  StateMachine sm;
  std::vector<std::string> log;
  EXPECT_EQ(handle(&sm, IncreaseCommand{1}, &log).mCode, 200u);
  EXPECT_EQ(handle(&sm, IncreaseCommand{1}, &log).mCode, 201u);
  EXPECT_EQ(handle(&sm, IncreaseCommand{5}, &log).mCode, 400u);
  EXPECT_EQ(sm.value(), 1);
  EXPECT_EQ(log.size(), 1u);
}

TEST(VariantEvents, DecreaseAndReset) {
  StateMachine sm;
  std::vector<std::string> log;
  for (int i = 1; i <= 3; ++i) {
    handle(&sm, IncreaseCommand{i}, &log);
  }
  EXPECT_EQ(handle(&sm, DecreaseCommand{-1}, &log).mMessage, "value must be non-negative");
  EXPECT_EQ(handle(&sm, DecreaseCommand{3}, &log).mCode, 201u);
  EXPECT_EQ(handle(&sm, DecreaseCommand{2}, &log).mCode, 200u);
  EXPECT_EQ(sm.value(), 2);
  EXPECT_EQ(handle(&sm, ResetCommand{}, &log).mCode, 200u);
  EXPECT_EQ(sm.value(), 0);
}

TEST(VariantEvents, EncodeUsesVariantIndex) {
  EXPECT_EQ(encode(Increased{7}), "0:7");
  EXPECT_EQ(encode(Decreased{6}), "1:6");
  EXPECT_EQ(encode(Reset{}), "2:");
}

TEST(VariantEvents, DecodeRoundTripsAndRejectsGarbage) {
  for (const Event &e : {Event{Increased{3}}, Event{Decreased{2}}, Event{Reset{}}}) {
    Event back = decode(encode(e));
    EXPECT_EQ(back.index(), e.index());
    EXPECT_EQ(encode(back), encode(e));
  }
  EXPECT_THROW(decode("9:1"), std::runtime_error);
  EXPECT_THROW(decode("nonsense"), std::runtime_error);
  EXPECT_TRUE(std::holds_alternative<Decreased>(decode("1:4")));
  EXPECT_EQ(std::get<Decreased>(decode("1:4")).newValue, 4);
  EXPECT_THROW(std::get<Increased>(decode("1:4")), std::bad_variant_access);
}

TEST(VariantEvents, ReplayIsDeterministic) {
  StateMachine sm;
  std::vector<std::string> log;
  for (int i = 1; i <= 5; ++i) {
    handle(&sm, IncreaseCommand{i}, &log);
  }
  handle(&sm, DecreaseCommand{4}, &log);
  handle(&sm, ResetCommand{}, &log);
  handle(&sm, IncreaseCommand{1}, &log);
  ASSERT_EQ(log.size(), 8u) << "5 increases, 1 decrease, 1 reset, 1 increase";
  StateMachine recovered = replay(log);
  EXPECT_EQ(recovered.value(), sm.value());
  EXPECT_EQ(replay(log).value(), recovered.value());
}

// ---- Virtual vs variant, for comparison (printed, not asserted) ------------
namespace {
struct VEvent {
  virtual ~VEvent() = default;
  virtual int applyTo(int v) const = 0;
};
struct VInc final : VEvent {
  explicit VInc(int n) : n(n) {}
  int applyTo(int) const override { return n; }
  int n;
};
struct VReset final : VEvent {
  int applyTo(int) const override { return 0; }
};
}  // namespace

TEST(VariantEvents, CompareWithVirtualDispatch) {
  constexpr int kN = 200000;
  std::vector<std::unique_ptr<VEvent>> virt;
  std::vector<Event> var;
  virt.reserve(kN);
  var.reserve(kN);
  for (int i = 0; i < kN; ++i) {
    if (i % 10 == 9) {
      virt.push_back(std::make_unique<VReset>());
      var.emplace_back(Reset{});
    } else {
      virt.push_back(std::make_unique<VInc>(i));
      var.emplace_back(Increased{i});
    }
  }
  auto t0 = std::chrono::steady_clock::now();
  int a = 0;
  for (const auto &e : virt) {
    a = e->applyTo(a);
  }
  auto t1 = std::chrono::steady_clock::now();
  StateMachine sm;
  for (const auto &e : var) {
    sm.apply(e);
  }
  auto t2 = std::chrono::steady_clock::now();
  using us = std::chrono::microseconds;
  std::printf("  virtual (one heap object each): %lld us\n  variant (contiguous values)   : %lld us\n",
              static_cast<long long>(std::chrono::duration_cast<us>(t1 - t0).count()),
              static_cast<long long>(std::chrono::duration_cast<us>(t2 - t1).count()));
  EXPECT_EQ(a, sm.value());
}
