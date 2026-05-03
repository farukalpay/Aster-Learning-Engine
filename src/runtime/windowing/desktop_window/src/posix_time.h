#define DESKTOP_WINDOW_POSIX_LIBRARY_TIMER_STATE _DESKTOP_WINDOWtimerPOSIX posix;

#include <stdint.h>
#include <time.h>


// POSIX-specific global timer data
//
typedef struct _DESKTOP_WINDOWtimerPOSIX
{
    clockid_t   clock;
    uint64_t    frequency;
} _DESKTOP_WINDOWtimerPOSIX;

