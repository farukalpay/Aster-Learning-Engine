// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <chrono>

#ifndef ASTER_HAS_PROFILING_RUNTIME
#define ASTER_HAS_PROFILING_RUNTIME 0
#endif

namespace aster::profile {

bool startCapture();
bool stopCapture();
bool saveCapture(const char *path);
void shutdown();
void record(const char *name, double seconds);

class Scope {
public:
  explicit Scope(const char *name) : name_(name), start_(std::chrono::steady_clock::now()) {}

  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;

  ~Scope() {
    const auto end = std::chrono::steady_clock::now();
    record(name_, std::chrono::duration<double>(end - start_).count());
  }

private:
  const char *name_ = "";
  std::chrono::steady_clock::time_point start_;
};

} // namespace aster::profile

#if ASTER_HAS_PROFILING_RUNTIME
#define ASTER_PROFILE_JOIN_INNER(A, B) A##B
#define ASTER_PROFILE_JOIN(A, B) ASTER_PROFILE_JOIN_INNER(A, B)
#define ASTER_PROFILE_FRAME(NAME) ::aster::profile::Scope ASTER_PROFILE_JOIN(_aster_frame_, __LINE__)(NAME)
#define ASTER_PROFILE_SCOPE(NAME) ::aster::profile::Scope ASTER_PROFILE_JOIN(_aster_scope_, __LINE__)(NAME)
#define ASTER_PROFILE_FUNCTION() ASTER_PROFILE_SCOPE(__func__)
#else
#define ASTER_PROFILE_FRAME(NAME) ((void)0)
#define ASTER_PROFILE_SCOPE(NAME) ((void)0)
#define ASTER_PROFILE_FUNCTION() ((void)0)
#endif
