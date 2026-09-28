#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <type_traits>

#include "raii.h"

using namespace j2c;  // NOLINT

TEST(Raii, LiveCounterTracksScope) {
  EXPECT_EQ(LiveCounter::alive(), 0);
  {
    LiveCounter a;
    EXPECT_EQ(LiveCounter::alive(), 1);
    {
      LiveCounter b;
      LiveCounter c;
      EXPECT_EQ(LiveCounter::alive(), 3);
    }
    EXPECT_EQ(LiveCounter::alive(), 1);
  }
  EXPECT_EQ(LiveCounter::alive(), 0);
}

TEST(Raii, LiveCounterReleasedDuringException) {
  try {
    LiveCounter a;
    throw std::runtime_error("boom");
  } catch (const std::exception &) {
  }
  EXPECT_EQ(LiveCounter::alive(), 0);
}

TEST(Raii, ScopeGuardRunsOnExit) {
  int calls = 0;
  {
    ScopeGuard g([&calls] { ++calls; });
    EXPECT_EQ(calls, 0);
  }
  EXPECT_EQ(calls, 1);
}

TEST(Raii, ScopeGuardRunsOnException) {
  int calls = 0;
  try {
    ScopeGuard g([&calls] { ++calls; });
    throw std::runtime_error("boom");
  } catch (...) {
  }
  EXPECT_EQ(calls, 1);
}

TEST(Raii, ScopeGuardDismissed) {
  int calls = 0;
  {
    ScopeGuard g([&calls] { ++calls; });
    g.dismiss();
  }
  EXPECT_EQ(calls, 0);
}

TEST(Raii, ScopeGuardIsNotCopyable) {
  EXPECT_FALSE(std::is_copy_constructible_v<ScopeGuard>);
}

static std::string readAll(const std::string &path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

TEST(Raii, FileHandleWritesAndCloses) {
  const std::string path = ::testing::TempDir() + "j2c_raii.txt";
  {
    FileHandle f(path, "w");
    EXPECT_TRUE(f.isOpen());
    f.write("hello ");
    f.write("raii");
  }  // closed (and flushed) here
  EXPECT_EQ(readAll(path), "hello raii");
  std::remove(path.c_str());
}

TEST(Raii, FileHandleThrowsWhenOpenFails) {
  EXPECT_THROW(FileHandle("/no/such/dir/file.txt", "r"), std::runtime_error);
}

TEST(Raii, FileHandleIsMoveOnly) {
  EXPECT_FALSE(std::is_copy_constructible_v<FileHandle>);
  EXPECT_FALSE(std::is_copy_assignable_v<FileHandle>);
  EXPECT_TRUE(std::is_move_constructible_v<FileHandle>);
}

TEST(Raii, FileHandleMoveTransfersOwnership) {
  const std::string path = ::testing::TempDir() + "j2c_raii_move.txt";
  {
    FileHandle a(path, "w");
    FileHandle b(std::move(a));
    EXPECT_FALSE(a.isOpen());  // NOLINT(bugprone-use-after-move): intentional
    EXPECT_TRUE(b.isOpen());
    b.write("moved");
  }
  EXPECT_EQ(readAll(path), "moved");
  std::remove(path.c_str());
}
