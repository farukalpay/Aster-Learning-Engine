#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_WIN32_TIMER)

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowPlatformInitTimer(void)
{
    QueryPerformanceFrequency((LARGE_INTEGER*) &_desktop_window.timer.win32.frequency);
}

uint64_t _desktop_windowPlatformGetTimerValue(void)
{
    uint64_t value;
    QueryPerformanceCounter((LARGE_INTEGER*) &value);
    return value;
}

uint64_t _desktop_windowPlatformGetTimerFrequency(void)
{
    return _desktop_window.timer.win32.frequency;
}

#endif // DESKTOP_WINDOW_BUILD_WIN32_TIMER

