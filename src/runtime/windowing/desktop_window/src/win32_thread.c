#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_WIN32_THREAD)

#include <assert.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowPlatformCreateTls(_DESKTOP_WINDOWtls* tls)
{
    assert(tls->win32.allocated == DESKTOP_WINDOW_FALSE);

    tls->win32.index = TlsAlloc();
    if (tls->win32.index == TLS_OUT_OF_INDEXES)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Win32: Failed to allocate TLS index");
        return DESKTOP_WINDOW_FALSE;
    }

    tls->win32.allocated = DESKTOP_WINDOW_TRUE;
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowPlatformDestroyTls(_DESKTOP_WINDOWtls* tls)
{
    if (tls->win32.allocated)
        TlsFree(tls->win32.index);
    memset(tls, 0, sizeof(_DESKTOP_WINDOWtls));
}

void* _desktop_windowPlatformGetTls(_DESKTOP_WINDOWtls* tls)
{
    assert(tls->win32.allocated == DESKTOP_WINDOW_TRUE);
    return TlsGetValue(tls->win32.index);
}

void _desktop_windowPlatformSetTls(_DESKTOP_WINDOWtls* tls, void* value)
{
    assert(tls->win32.allocated == DESKTOP_WINDOW_TRUE);
    TlsSetValue(tls->win32.index, value);
}

DESKTOP_WINDOWbool _desktop_windowPlatformCreateMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->win32.allocated == DESKTOP_WINDOW_FALSE);
    InitializeCriticalSection(&mutex->win32.section);
    return mutex->win32.allocated = DESKTOP_WINDOW_TRUE;
}

void _desktop_windowPlatformDestroyMutex(_DESKTOP_WINDOWmutex* mutex)
{
    if (mutex->win32.allocated)
        DeleteCriticalSection(&mutex->win32.section);
    memset(mutex, 0, sizeof(_DESKTOP_WINDOWmutex));
}

void _desktop_windowPlatformLockMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->win32.allocated == DESKTOP_WINDOW_TRUE);
    EnterCriticalSection(&mutex->win32.section);
}

void _desktop_windowPlatformUnlockMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->win32.allocated == DESKTOP_WINDOW_TRUE);
    LeaveCriticalSection(&mutex->win32.section);
}

#endif // DESKTOP_WINDOW_BUILD_WIN32_THREAD

