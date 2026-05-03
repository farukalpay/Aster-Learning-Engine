#ifndef NETIO_DETAIL_IMPL_POSIX_MUTEX_IPP
#define NETIO_DETAIL_IMPL_POSIX_MUTEX_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include "netio/detail/posix_mutex.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

posix_mutex::posix_mutex()
{
  int error = ::pthread_mutex_init(&mutex_, 0);
  netio::error_code ec(error,
      netio::error::get_system_category());
  netio::detail::throw_error(ec, "mutex");
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_IMPL_POSIX_MUTEX_IPP
