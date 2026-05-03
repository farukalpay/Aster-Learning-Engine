#ifndef NETIO_DETAIL_STD_STATIC_MUTEX_HPP
#define NETIO_DETAIL_STD_STATIC_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <mutex>
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class std_event;

class std_static_mutex
  : private noncopyable
{
public:
  typedef netio::detail::scoped_lock<std_static_mutex> scoped_lock;

  // Constructor.
  std_static_mutex(int)
  {
  }

  // Destructor.
  ~std_static_mutex()
  {
  }

  // Initialise the mutex.
  void init()
  {
    // Nothing to do.
  }

  // Lock the mutex.
  void lock()
  {
    mutex_.lock();
  }

  // Unlock the mutex.
  void unlock()
  {
    mutex_.unlock();
  }

private:
  friend class std_event;
  std::mutex mutex_;
};

#define NETIO_STD_STATIC_MUTEX_INIT 0

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_STD_STATIC_MUTEX_HPP
