// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#ifndef ASTER_HAS_PROFILING_RUNTIME
#define ASTER_HAS_PROFILING_RUNTIME 0
#endif

#if ASTER_HAS_PROFILING_RUNTIME
#include <profile_runtime.h>
#endif

namespace aster::profile {

inline bool startCapture() {
#if ASTER_HAS_PROFILING_RUNTIME
  return ProfileRuntime::StartCapture();
#else
  return false;
#endif
}

inline bool stopCapture() {
#if ASTER_HAS_PROFILING_RUNTIME
  return ProfileRuntime::StopCapture();
#else
  return false;
#endif
}

inline bool saveCapture(const char *path) {
#if ASTER_HAS_PROFILING_RUNTIME
  return ProfileRuntime::SaveCapture(path);
#else
  (void)path;
  return false;
#endif
}

inline void shutdown() {
#if ASTER_HAS_PROFILING_RUNTIME
  ProfileRuntime::Shutdown();
#endif
}

} // namespace aster::profile

#if ASTER_HAS_PROFILING_RUNTIME
#define ASTER_PROFILE_FRAME(NAME) PROFILE_RUNTIME_FRAME(NAME)
#define ASTER_PROFILE_SCOPE(NAME) PROFILE_RUNTIME_EVENT(NAME)
#define ASTER_PROFILE_FUNCTION() PROFILE_RUNTIME_EVENT()
#else
#define ASTER_PROFILE_FRAME(NAME) ((void)0)
#define ASTER_PROFILE_SCOPE(NAME) ((void)0)
#define ASTER_PROFILE_FUNCTION() ((void)0)
#endif
