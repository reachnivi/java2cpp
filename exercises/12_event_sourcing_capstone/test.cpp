#include <gtest/gtest.h>

#include "mini_es.h"

using namespace j2c::es;  // NOLINT

class CapstoneTest : public ::testing::Test {
 protected:
  EventLog mLog;
  App mApp{mLog};
};

// ---- Baseline: already green on the starter code. Read these first. ----

TEST_F(CapstoneTest, IncreaseHappyPath) {
  EXPECT_EQ(mApp.handle(IncreaseCommand(1)).mCode, 200u);
  EXPECT_EQ(mApp.handle(IncreaseCommand(2)).mCode, 200u);
  EXPECT_EQ(mApp.state().getValue(), 2);
  EXPECT_EQ(mLog.size(), 2u);
}

TEST_F(CapstoneTest, IncreaseIsIdempotentAndValidated) {
  mApp.handle(IncreaseCommand(1));
  auto dup = mApp.handle(IncreaseCommand(1));
  EXPECT_EQ(dup.mCode, 201u);
  EXPECT_EQ(dup.mMessage, "Duplicated request");
  EXPECT_EQ(mApp.handle(IncreaseCommand(5)).mCode, 400u);
  EXPECT_EQ(mApp.state().getValue(), 1);
  EXPECT_EQ(mLog.size(), 1u) << "rejected commands must not produce events";
}

// ---- Your feature ----

TEST_F(CapstoneTest, DecreaseVerifyRejectsNegative) {
  DecreaseCommand cmd(-1);
  EXPECT_EQ(cmd.verifyCommand(), "value must be non-negative");
  auto hint = mApp.handle(cmd);
  EXPECT_EQ(hint.mCode, 400u);
  EXPECT_EQ(hint.mMessage, "value must be non-negative");
  EXPECT_EQ(mLog.size(), 0u);
}

TEST_F(CapstoneTest, DecreaseHappyPath) {
  for (int i = 1; i <= 3; ++i) {
    mApp.handle(IncreaseCommand(i));
  }
  auto hint = mApp.handle(DecreaseCommand(2));
  EXPECT_EQ(hint.mCode, 200u);
  EXPECT_EQ(hint.mMessage, "Success");
  EXPECT_EQ(mApp.state().getValue(), 2);
  EXPECT_EQ(mLog.size(), 4u);
  EXPECT_EQ(mLog.entries().back().type, DecreasedEvent::kType);
  EXPECT_EQ(mLog.entries().back().payload, "2");
}

TEST_F(CapstoneTest, DecreaseDuplicateAndInvalid) {
  for (int i = 1; i <= 3; ++i) {
    mApp.handle(IncreaseCommand(i));
  }
  EXPECT_EQ(mApp.handle(DecreaseCommand(3)).mCode, 201u);
  EXPECT_EQ(mApp.handle(DecreaseCommand(0)).mCode, 400u);
  EXPECT_EQ(mApp.state().getValue(), 3);
  EXPECT_EQ(mLog.size(), 3u);
}

TEST(DecodeTest, DecodesBothEventTypesAndRejectsUnknown) {
  auto p = decodeEvent(ProcessedEvent::kType, "7");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getType(), ProcessedEvent::kType);
  auto d = decodeEvent(DecreasedEvent::kType, "6");
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->getType(), DecreasedEvent::kType);
  EXPECT_EQ(d->encodeToString(), "6");
  EXPECT_THROW(decodeEvent(99, "1"), std::runtime_error);
}

TEST_F(CapstoneTest, RecoveryReplaysToIdenticalState) {
  for (int i = 1; i <= 5; ++i) {
    mApp.handle(IncreaseCommand(i));
  }
  mApp.handle(DecreaseCommand(4));
  mApp.handle(DecreaseCommand(3));
  mApp.handle(IncreaseCommand(4));
  ASSERT_EQ(mApp.state().getValue(), 4);

  StateMachine recovered = App::recover(mLog);
  EXPECT_TRUE(recovered.hasSameState(mApp.state()));

  // Determinism: replaying twice gives the same answer.
  EXPECT_TRUE(App::recover(mLog).hasSameState(recovered));
}

TEST(RecoveryTest, EmptyLogGivesInitialState) {
  EventLog log;
  EXPECT_EQ(App::recover(log).getValue(), 0);
}
