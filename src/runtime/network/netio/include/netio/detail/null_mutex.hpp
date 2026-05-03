#ifndef NETIO_DETAIL_NULL_MUTEX_HPP
#define NETIO_DETAIL_NULL_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/noncopyable.hpp"
#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class null_mutex
  : private noncopyable
{
public:
  typedef netio::detail::scoped_lock<null_mutex> scoped_lock;

  // Constructor.
  null_mutex()
  {
  }

  // Destructor.
  ~null_mutex()
  {
  }

  // Try to lock the mutex.
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
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_NULL_MUTEX_HPP
