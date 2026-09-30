#ifndef CRASH_LAB_16_CRASH_HANDLER_H_
#define CRASH_LAB_16_CRASH_HANDLER_H_

// A minimal fatal-signal handler: print a backtrace to stderr, then re-raise
// the signal with the default action so a core file is STILL produced.
//
// Rules inside a signal handler: only async-signal-safe calls. No malloc, no
// printf, no locks, no spdlog. backtrace() is not formally safe (it may
// allocate on first use), which is why we call it once at install time to
// warm it up; backtrace_symbols_fd() writes straight to an fd without malloc.
// Production-grade alternatives: Abseil's failure signal handler (Gringofts
// already vendors abseil), glog's InstallFailureSignalHandler, backward-cpp.

#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

namespace crash_handler_detail {

inline void writeStr(const char *s) {
  ssize_t unused = ::write(STDERR_FILENO, s, strlen(s));
  (void)unused;
}

inline void onFatalSignal(int sig, siginfo_t *info, void * /*ucontext*/) {
  writeStr("\n*** FATAL SIGNAL ");
  writeStr(strsignal(sig));  // not strictly async-signal-safe; fine on glibc
  if (sig == SIGSEGV || sig == SIGBUS) {
    char buf[64];
    // hand-rolled hex to avoid snprintf
    auto addr = reinterpret_cast<unsigned long>(info->si_addr);
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    do {
      *--p = "0123456789abcdef"[addr & 0xF];
      addr >>= 4;
    } while (addr != 0);
    writeStr(" at address 0x");
    writeStr(p);
  }
  writeStr(" ***\nbacktrace (most recent call first):\n");
  void *frames[64];
  int n = backtrace(frames, 64);
  backtrace_symbols_fd(frames, n, STDERR_FILENO);
  writeStr("*** end of backtrace; re-raising for core dump ***\n");

  // Restore the default action and re-raise so the kernel writes a core.
  signal(sig, SIG_DFL);
  raise(sig);
}

}  // namespace crash_handler_detail

inline void installCrashHandler() {
  void *warmup[1];
  backtrace(warmup, 1);  // force libgcc to load now, not inside the handler

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_handler_detail::onFatalSignal;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) {
    sigaction(sig, &sa, nullptr);
  }
}

#endif  // CRASH_LAB_16_CRASH_HANDLER_H_
