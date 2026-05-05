// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/profiler.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace aster::profile {
namespace {

struct Event {
  std::string name;
  double seconds = 0.0;
};

std::mutex &eventMutex() {
  static std::mutex mutex;
  return mutex;
}

std::vector<Event> &events() {
  static std::vector<Event> recorded;
  return recorded;
}

bool &capturing() {
  static bool active = false;
  return active;
}

constexpr std::size_t kMaxEvents = 8192u;

} // namespace

bool startCapture() {
  std::lock_guard lock(eventMutex());
  events().clear();
  capturing() = true;
  return true;
}

bool stopCapture() {
  std::lock_guard lock(eventMutex());
  capturing() = false;
  return true;
}

bool saveCapture(const char *path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  std::vector<Event> snapshot;
  {
    std::lock_guard lock(eventMutex());
    snapshot = events();
  }
  std::ofstream file(path);
  if (!file) {
    return false;
  }
  file << "name,seconds\n";
  for (const Event &event : snapshot) {
    file << event.name << ',' << event.seconds << '\n';
  }
  return true;
}

void shutdown() {
  std::lock_guard lock(eventMutex());
  capturing() = false;
  events().clear();
}

void record(const char *name, const double seconds) {
  std::lock_guard lock(eventMutex());
  if (!capturing()) {
    return;
  }
  std::vector<Event> &out = events();
  if (out.size() >= kMaxEvents) {
    out.erase(out.begin());
  }
  out.push_back({name == nullptr ? "" : name, std::max(seconds, 0.0)});
}

} // namespace aster::profile
