#include <pthread.h>

#define DESKTOP_WINDOW_POSIX_TLS_STATE    _DESKTOP_WINDOWtlsPOSIX   posix;
#define DESKTOP_WINDOW_POSIX_MUTEX_STATE  _DESKTOP_WINDOWmutexPOSIX posix;


// POSIX-specific thread local storage data
//
typedef struct _DESKTOP_WINDOWtlsPOSIX
{
    DESKTOP_WINDOWbool        allocated;
    pthread_key_t   key;
} _DESKTOP_WINDOWtlsPOSIX;

// POSIX-specific mutex data
//
typedef struct _DESKTOP_WINDOWmutexPOSIX
{
    DESKTOP_WINDOWbool        allocated;
    pthread_mutex_t handle;
} _DESKTOP_WINDOWmutexPOSIX;

