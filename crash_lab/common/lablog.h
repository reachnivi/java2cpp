#ifndef CRASH_LAB_COMMON_LABLOG_H_
#define CRASH_LAB_COMMON_LABLOG_H_

// A tiny stand-in for SPDLOG_INFO with Gringofts' log pattern:
//   [time] [file:line func] [level] [thread id] message
// Writes straight to stderr (unbuffered), so lines logged just before a crash
// are not lost. See DEBUGGING.md, "Logs vanish right before the crash".

#include <sys/syscall.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <ctime>

#ifndef __FILE_NAME__  // GCC < 12
#define __FILE_NAME__ __FILE__
#endif

#define LAB_LOG(level, ...)                                                                   \
  do {                                                                                        \
    auto _now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());      \
    char _ts[16];                                                                             \
    std::strftime(_ts, sizeof(_ts), "%H:%M:%S", std::localtime(&_now));                       \
    std::fprintf(stderr, "[%s] [%s:%d %s] [%s] [thread %ld] ", _ts, __FILE_NAME__, __LINE__, \
                 __func__, level, static_cast<long>(::syscall(SYS_gettid)));                  \
    std::fprintf(stderr, __VA_ARGS__);                                                        \
    std::fprintf(stderr, "\n");                                                               \
  } while (0)

#define LOG_INFO(...) LAB_LOG("info", __VA_ARGS__)
#define LOG_WARN(...) LAB_LOG("warning", __VA_ARGS__)
#define LOG_ERROR(...) LAB_LOG("error", __VA_ARGS__)

#endif  // CRASH_LAB_COMMON_LABLOG_H_
