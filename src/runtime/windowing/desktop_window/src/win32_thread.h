#undef APIENTRY

#include <windows.h>

#define DESKTOP_WINDOW_WIN32_TLS_STATE            _DESKTOP_WINDOWtlsWin32     win32;
#define DESKTOP_WINDOW_WIN32_MUTEX_STATE          _DESKTOP_WINDOWmutexWin32   win32;

// Win32-specific thread local storage data
//
typedef struct _DESKTOP_WINDOWtlsWin32
{
    DESKTOP_WINDOWbool            allocated;
    DWORD               index;
} _DESKTOP_WINDOWtlsWin32;

// Win32-specific mutex data
//
typedef struct _DESKTOP_WINDOWmutexWin32
{
    DESKTOP_WINDOWbool            allocated;
    CRITICAL_SECTION    section;
} _DESKTOP_WINDOWmutexWin32;

