#define _GNU_SOURCE

#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_POSIX_POLL)

#include <signal.h>
#include <time.h>
#include <errno.h>

DESKTOP_WINDOWbool _desktop_windowPollPOSIX(struct pollfd* fds, nfds_t count, double* timeout)
{
    for (;;)
    {
        if (timeout)
        {
            const uint64_t base = _desktop_windowPlatformGetTimerValue();

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__CYGWIN__)
            const time_t seconds = (time_t) *timeout;
            const long nanoseconds = (long) ((*timeout - seconds) * 1e9);
            const struct timespec ts = { seconds, nanoseconds };
            const int result = ppoll(fds, count, &ts, NULL);
#elif defined(__NetBSD__)
            const time_t seconds = (time_t) *timeout;
            const long nanoseconds = (long) ((*timeout - seconds) * 1e9);
            const struct timespec ts = { seconds, nanoseconds };
            const int result = pollts(fds, count, &ts, NULL);
#else
            const int milliseconds = (int) (*timeout * 1e3);
            const int result = poll(fds, count, milliseconds);
#endif
            const int error = errno; // clock_gettime may overwrite our error

            *timeout -= (_desktop_windowPlatformGetTimerValue() - base) /
                (double) _desktop_windowPlatformGetTimerFrequency();

            if (result > 0)
                return DESKTOP_WINDOW_TRUE;
            else if (result == -1 && error != EINTR && error != EAGAIN)
                return DESKTOP_WINDOW_FALSE;
            else if (*timeout <= 0.0)
                return DESKTOP_WINDOW_FALSE;
        }
        else
        {
            const int result = poll(fds, count, -1);
            if (result > 0)
                return DESKTOP_WINDOW_TRUE;
            else if (result == -1 && errno != EINTR && errno != EAGAIN)
                return DESKTOP_WINDOW_FALSE;
        }
    }
}

#endif // DESKTOP_WINDOW_BUILD_POSIX_POLL

