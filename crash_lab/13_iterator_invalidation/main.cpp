// Lab 13: session expiry "works" in tests but crashes or silently skips
// entries in production. In Java this would at least throw
// ConcurrentModificationException. C++ gives you no such courtesy.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "lablog.h"

struct Session {
  std::string client;
  uint64_t lastSeenMs;
};

void expireSessions(std::map<uint64_t, Session> *sessions, uint64_t nowMs, uint64_t ttlMs) {
  for (auto it = sessions->begin(); it != sessions->end(); ++it) {
    if (nowMs - it->second.lastSeenMs > ttlMs) {
      LOG_INFO("expiring session %lu (%s)", static_cast<unsigned long>(it->first), it->second.client.c_str());
      sessions->erase(it);
    }
  }
}

void dropSlowPeers(std::vector<uint64_t> *latenciesMs, uint64_t maxMs) {
  for (auto it = latenciesMs->begin(); it != latenciesMs->end(); ++it) {
    if (*it > maxMs) {
      latenciesMs->erase(it);
    }
  }
}

int main() {
  std::map<uint64_t, Session> sessions;
  for (uint64_t id = 1; id <= 20; ++id) {
    sessions[id] = Session{"client-" + std::to_string(id) + "-with-a-long-name-to-force-heap", id % 3 == 0 ? 0ULL : 900ULL};
  }
  expireSessions(&sessions, 1000, 500);
  LOG_INFO("sessions left: %zu (expected 14)", sessions.size());

  std::vector<uint64_t> latencies{5, 900, 950, 7, 800};
  dropSlowPeers(&latencies, 100);
  LOG_INFO("peers left: %zu (expected 2)", latencies.size());
}
