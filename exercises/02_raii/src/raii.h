#ifndef J2C_RAII_H_
#define J2C_RAII_H_

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>

namespace j2c {

class LiveCounter {
 public:
  LiveCounter() { /* TODO */ }
  ~LiveCounter() { /* TODO */ }

  static int alive() { return sAlive; }

 private:
  // C++17 "inline variable": a static data member defined in the header.
  inline static int sAlive = 0;
};

class ScopeGuard {
 public:
  explicit ScopeGuard(std::function<void()> onExit) : mOnExit(std::move(onExit)) {}
  ~ScopeGuard() {
    // TODO: call mOnExit unless dismissed
  }

  // TODO: delete the copy constructor and copy assignment operator.

  void dismiss() { /* TODO */ }

 private:
  std::function<void()> mOnExit;
};

class FileHandle {
 public:
  FileHandle(const std::string &path, const char *mode) {
    // TODO: std::fopen; throw std::runtime_error on failure
    (void)path;
    (void)mode;
  }
  ~FileHandle() {
    // TODO: std::fclose if open
  }

  // TODO: delete copy ctor / copy assignment.
  // TODO: implement move ctor: steal other.mFile, set other.mFile = nullptr.
  // TODO: implement move assignment (close your own file first!).

  bool isOpen() const { return mFile != nullptr; }

  void write(const std::string &s) {
    // TODO: std::fputs(s.c_str(), mFile)
    (void)s;
  }

 private:
  std::FILE *mFile = nullptr;
};

}  // namespace j2c

#endif  // J2C_RAII_H_
