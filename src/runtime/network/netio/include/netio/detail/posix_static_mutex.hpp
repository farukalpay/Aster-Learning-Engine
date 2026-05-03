#ifndef NETIO_DETAIL_POSIX_STATIC_MUTEX_HPP
#define NETIO_DETAIL_POSIX_STATIC_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include <pthread.h>
#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

struct posix_static_mutex
{
  typedef netio::detail::scoped_lock<posix_static_mutex> scoped_lock;

  // Initialise the mutex.
  void init()
  {
    // Nothing to do.
  }

  // Lock the mutex.
  void lock()
  {
    (void)::pthread_mutex_lock(&mutex_); // Ignore EINVAL.
  }

  // Unlock the mutex.
  void unlock()
  {
    (void)::pthread_mutex_unlock(&mutex_); // Ignore EINVAL.
  }

  ::pthread_mutex_t mutex_;
};

#define NETIO_POSIX_STATIC_MUTEX_INIT { PTHREAD_MUTEX_INITIALIZER }

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_POSIX_STATIC_MUTEX_HPP
