#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_POSIX_TIMER)

#include <unistd.h>
#include <sys/time.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowPlatformInitTimer(void)
{
    _desktop_window.timer.posix.clock = CLOCK_REALTIME;
    _desktop_window.timer.posix.frequency = 1000000000;

#if defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        _desktop_window.timer.posix.clock = CLOCK_MONOTONIC;
#endif
}

uint64_t _desktop_windowPlatformGetTimerValue(void)
{
    struct timespec ts;
    clock_gettime(_desktop_window.timer.posix.clock, &ts);
    return (uint64_t) ts.tv_sec * _desktop_window.timer.posix.frequency + (uint64_t) ts.tv_nsec;
}

uint64_t _desktop_windowPlatformGetTimerFrequency(void)
{
    return _desktop_window.timer.posix.frequency;
}

#endif // DESKTOP_WINDOW_BUILD_POSIX_TIMER

