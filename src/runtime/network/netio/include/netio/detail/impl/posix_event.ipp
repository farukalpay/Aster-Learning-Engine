#ifndef NETIO_DETAIL_IMPL_POSIX_EVENT_IPP
#define NETIO_DETAIL_IMPL_POSIX_EVENT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include "netio/detail/posix_event.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

posix_event::posix_event()
  : state_(0)
{
#if (defined(__MACH__) && defined(__APPLE__)) \
      || (defined(__ANDROID__) && (__ANDROID_API__ < 21))
  int error = ::pthread_cond_init(&cond_, 0);
#else // (defined(__MACH__) && defined(__APPLE__))
      // || (defined(__ANDROID__) && (__ANDROID_API__ < 21))
  ::pthread_condattr_t attr;
  int error = ::pthread_condattr_init(&attr);
  if (error == 0)
  {
    error = ::pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (error == 0)
      error = ::pthread_cond_init(&cond_, &attr);
    ::pthread_condattr_destroy(&attr);
  }
#endif // (defined(__MACH__) && defined(__APPLE__))
       // || (defined(__ANDROID__) && (__ANDROID_API__ < 21))

  netio::error_code ec(error,
      netio::error::get_system_category());
  netio::detail::throw_error(ec, "event");
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_IMPL_POSIX_EVENT_IPP
