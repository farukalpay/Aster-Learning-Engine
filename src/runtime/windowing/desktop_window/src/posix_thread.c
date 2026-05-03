#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_POSIX_THREAD)

#include <assert.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowPlatformCreateTls(_DESKTOP_WINDOWtls* tls)
{
    assert(tls->posix.allocated == DESKTOP_WINDOW_FALSE);

    if (pthread_key_create(&tls->posix.key, NULL) != 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "POSIX: Failed to create context TLS");
        return DESKTOP_WINDOW_FALSE;
    }

    tls->posix.allocated = DESKTOP_WINDOW_TRUE;
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowPlatformDestroyTls(_DESKTOP_WINDOWtls* tls)
{
    if (tls->posix.allocated)
        pthread_key_delete(tls->posix.key);
    memset(tls, 0, sizeof(_DESKTOP_WINDOWtls));
}

void* _desktop_windowPlatformGetTls(_DESKTOP_WINDOWtls* tls)
{
    assert(tls->posix.allocated == DESKTOP_WINDOW_TRUE);
    return pthread_getspecific(tls->posix.key);
}

void _desktop_windowPlatformSetTls(_DESKTOP_WINDOWtls* tls, void* value)
{
    assert(tls->posix.allocated == DESKTOP_WINDOW_TRUE);
    pthread_setspecific(tls->posix.key, value);
}

DESKTOP_WINDOWbool _desktop_windowPlatformCreateMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->posix.allocated == DESKTOP_WINDOW_FALSE);

    if (pthread_mutex_init(&mutex->posix.handle, NULL) != 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "POSIX: Failed to create mutex");
        return DESKTOP_WINDOW_FALSE;
    }

    return mutex->posix.allocated = DESKTOP_WINDOW_TRUE;
}

void _desktop_windowPlatformDestroyMutex(_DESKTOP_WINDOWmutex* mutex)
{
    if (mutex->posix.allocated)
        pthread_mutex_destroy(&mutex->posix.handle);
    memset(mutex, 0, sizeof(_DESKTOP_WINDOWmutex));
}

void _desktop_windowPlatformLockMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->posix.allocated == DESKTOP_WINDOW_TRUE);
    pthread_mutex_lock(&mutex->posix.handle);
}

void _desktop_windowPlatformUnlockMutex(_DESKTOP_WINDOWmutex* mutex)
{
    assert(mutex->posix.allocated == DESKTOP_WINDOW_TRUE);
    pthread_mutex_unlock(&mutex->posix.handle);
}

#endif // DESKTOP_WINDOW_BUILD_POSIX_THREAD

