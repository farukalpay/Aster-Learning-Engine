#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_COCOA_TIMER)

#include <mach/mach_time.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowPlatformInitTimer(void)
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);

    _desktop_window.timer.ns.frequency = (info.denom * 1e9) / info.numer;
}

uint64_t _desktop_windowPlatformGetTimerValue(void)
{
    return mach_absolute_time();
}

uint64_t _desktop_windowPlatformGetTimerFrequency(void)
{
    return _desktop_window.timer.ns.frequency;
}

#endif // DESKTOP_WINDOW_BUILD_COCOA_TIMER

