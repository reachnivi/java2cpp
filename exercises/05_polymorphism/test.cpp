#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "events.h"

using namespace j2c;  // NOLINT

TEST(Polymorphism, InterfacesHaveVirtualDestructors) {
  EXPECT_TRUE(std::has_virtual_destructor_v<Encodable>);
  EXPECT_TRUE(std::has_virtual_destructor_v<Event>);
  EXPECT_TRUE(std::is_abstract_v<Encodable>);
  EXPECT_TRUE(std::is_abstract_v<Event>);
  EXPECT_TRUE(std::is_final_v<IncreasedEvent>);
}

TEST(Polymorphism, DynamicDispatchThroughBaseReference) {
  IncreasedEvent inc(100, 5);
  const Event &e = inc;
  EXPECT_EQ(e.getType(), 1u);
  EXPECT_EQ(e.getCreatedTimeInNanos(), 100u);
  EXPECT_EQ(e.encodeToString(), "inc:5");
  EXPECT_EQ(e.describe(), "Increased to 5");
}

TEST(Polymorphism, DefaultImplementationUsedWhenNotOverridden) {
  ResetEvent r(200);
  const Event &e = r;
  EXPECT_EQ(e.getType(), 2u);
  EXPECT_EQ(e.encodeToString(), "reset");
  EXPECT_EQ(e.describe(), "Event#2");
}

TEST(Polymorphism, HeterogeneousContainerOfUniquePtrs) {
  std::vector<std::unique_ptr<Event>> events;
  events.push_back(std::make_unique<IncreasedEvent>(1, 1));
  events.push_back(std::make_unique<ResetEvent>(2));
  events.push_back(std::make_unique<IncreasedEvent>(3, 2));
  std::string all;
  for (const auto &e : events) {
    all += e->encodeToString() + ";";
  }
  EXPECT_EQ(all, "inc:1;reset;inc:2;");
}

TEST(Polymorphism, FactoryDecodesByType) {
  auto e = decodeEvent(1, "inc:42");
  ASSERT_NE(e, nullptr);
  auto *inc = dynamic_cast<IncreasedEvent *>(e.get());
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getValue(), 42);
  EXPECT_EQ(dynamic_cast<ResetEvent *>(e.get()), nullptr);

  auto r = decodeEvent(2, "reset");
  ASSERT_NE(r, nullptr);
  EXPECT_NE(dynamic_cast<ResetEvent *>(r.get()), nullptr);
}

TEST(Polymorphism, FactoryRejectsUnknownType) {
  EXPECT_THROW(decodeEvent(999, "?"), std::runtime_error);
}

TEST(Polymorphism, EncodeDecodeRoundTrip) {
  IncreasedEvent original(0, 1234);
  auto decoded = decodeEvent(original.getType(), original.encodeToString());
  ASSERT_NE(decoded, nullptr);
  EXPECT_EQ(decoded->encodeToString(), original.encodeToString());
}
