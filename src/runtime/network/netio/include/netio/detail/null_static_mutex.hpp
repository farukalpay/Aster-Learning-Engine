#ifndef NETIO_DETAIL_NULL_STATIC_MUTEX_HPP
#define NETIO_DETAIL_NULL_STATIC_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)

#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

struct null_static_mutex
{
  typedef netio::detail::scoped_lock<null_static_mutex> scoped_lock;

  // Initialise the mutex.
  void init()
  {
  }

  // Try to lock the mutex without blocking.
  bool try_lock()
  {
    return true;
  }

  // Lock the mutex.
  void lock()
  {
  }

  // Unlock the mutex.
  void unlock()
  {
  }

  int unused_;
};

#define NETIO_NULL_STATIC_MUTEX_INIT { 0 }

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_HAS_THREADS)

#endif // NETIO_DETAIL_NULL_STATIC_MUTEX_HPP
