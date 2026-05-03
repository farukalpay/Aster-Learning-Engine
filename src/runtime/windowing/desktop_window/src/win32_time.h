#undef APIENTRY

#include <windows.h>

#define DESKTOP_WINDOW_WIN32_LIBRARY_TIMER_STATE  _DESKTOP_WINDOWtimerWin32   win32;

// Win32-specific global timer data
//
typedef struct _DESKTOP_WINDOWtimerWin32
{
    uint64_t            frequency;
} _DESKTOP_WINDOWtimerWin32;

