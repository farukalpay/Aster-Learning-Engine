#ifndef NETIO_DETAIL_POSIX_MUTEX_HPP
#define NETIO_DETAIL_POSIX_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include <pthread.h>
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class posix_event;

class posix_mutex
  : private noncopyable
{
public:
  typedef netio::detail::scoped_lock<posix_mutex> scoped_lock;

  // Constructor.
  NETIO_DECL posix_mutex();

  // Destructor.
  ~posix_mutex()
  {
    ::pthread_mutex_destroy(&mutex_); // Ignore EBUSY.
  }

  // Try to lock the mutex.
  bool try_lock()
  {
    return ::pthread_mutex_trylock(&mutex_) == 0; // Ignore EINVAL.
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

private:
  friend class posix_event;
  ::pthread_mutex_t mutex_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/posix_mutex.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_POSIX_MUTEX_HPP
