#ifndef J2C_RAII_H_
#define J2C_RAII_H_

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>

namespace j2c {

class LiveCounter {
 public:
  LiveCounter() { ++sAlive; }
  ~LiveCounter() { --sAlive; }

  static int alive() { return sAlive; }

 private:
  inline static int sAlive = 0;
};

class ScopeGuard {
 public:
  explicit ScopeGuard(std::function<void()> onExit) : mOnExit(std::move(onExit)) {}
  ~ScopeGuard() {
    if (!mDismissed && mOnExit) {
      mOnExit();
    }
  }

  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;

  void dismiss() { mDismissed = true; }

 private:
  std::function<void()> mOnExit;
  bool mDismissed = false;
};

class FileHandle {
 public:
  FileHandle(const std::string &path, const char *mode) : mFile(std::fopen(path.c_str(), mode)) {
    if (mFile == nullptr) {
      throw std::runtime_error("cannot open " + path);
    }
  }
  ~FileHandle() { close(); }

  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;

  FileHandle(FileHandle &&other) noexcept : mFile(other.mFile) { other.mFile = nullptr; }
  FileHandle &operator=(FileHandle &&other) noexcept {
    if (this != &other) {
      close();
      mFile = other.mFile;
      other.mFile = nullptr;
    }
    return *this;
  }

  bool isOpen() const { return mFile != nullptr; }

  void write(const std::string &s) {
    if (mFile == nullptr) {
      throw std::logic_error("write on closed FileHandle");
    }
    std::fputs(s.c_str(), mFile);
  }

 private:
  void close() noexcept {
    if (mFile != nullptr) {
      std::fclose(mFile);
      mFile = nullptr;
    }
  }

  std::FILE *mFile = nullptr;
};

}  // namespace j2c

#endif  // J2C_RAII_H_
