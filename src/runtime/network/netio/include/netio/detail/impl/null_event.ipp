#ifndef NETIO_DETAIL_IMPL_NULL_EVENT_IPP
#define NETIO_DETAIL_IMPL_NULL_EVENT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)
# include <thread>
#elif defined(NETIO_WINDOWS) || defined(__CYGWIN__)
# include "netio/detail/socket_types.hpp"
#else
# include <unistd.h>
# if defined(__hpux)
#  include <sys/time.h>
# endif
# if !defined(__hpux) || defined(__SELECT)
#  include <sys/select.h>
# endif
#endif

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

void null_event::do_wait()
{
#if defined(NETIO_WINDOWS_RUNTIME)
  std::this_thread::sleep_until((std::chrono::steady_clock::time_point::max)());
#elif defined(NETIO_WINDOWS) || defined(__CYGWIN__)
  ::Sleep(INFINITE);
#else
  ::pause();
#endif
}

void null_event::do_wait_for_usec(long usec)
{
#if defined(NETIO_WINDOWS_RUNTIME)
  std::this_thread::sleep_for(std::chrono::microseconds(usec));
#elif defined(NETIO_WINDOWS) || defined(__CYGWIN__)
  ::Sleep(usec / 1000);
#elif defined(__hpux) && defined(__SELECT)
  timespec ts;
  ts.tv_sec = usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  ::pselect(0, 0, 0, 0, &ts, 0);
#else
  timeval tv;
  tv.tv_sec = usec / 1000000;
  tv.tv_usec = usec % 1000000;
  ::select(0, 0, 0, 0, &tv);
#endif
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_IMPL_NULL_EVENT_IPP
