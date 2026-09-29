// Lab 03: when the config has a bad port, the process should log
// "failed to start: ..." and exit with status 1. Instead it aborts with
// "terminate called without an active exception" and a core dump, and the
// error is never logged. main() clearly catches std::exception... so what?
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include "lablog.h"

class App {
 public:
  explicit App(const std::string &port) {
    mHeartbeatThread = std::thread([this] {
      while (mRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    });
    // Validation happens after the background thread has already started.
    mPort = parsePort(port);
  }

  ~App() {
    mRunning = false;
    if (mHeartbeatThread.joinable()) {
      mHeartbeatThread.join();
    }
  }

  void run() { LOG_INFO("serving on %d", mPort); }

 private:
  static int parsePort(const std::string &s) {
    int p = std::stoi(s);  // throws std::invalid_argument on "abc"
    if (p <= 0 || p > 65535) {
      throw std::out_of_range("port out of range: " + s);
    }
    return p;
  }

  std::thread mHeartbeatThread;
  std::atomic<bool> mRunning{true};
  int mPort = 0;
};

int main(int argc, char **argv) {
  std::string port = argc > 1 ? argv[1] : "99999";
  try {
    App app(port);
    app.run();
  } catch (const std::exception &e) {
    LOG_ERROR("failed to start: %s", e.what());
    return 1;
  }
  return 0;
}
