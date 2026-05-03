#ifndef NETIO_DETAIL_STD_MUTEX_HPP
#define NETIO_DETAIL_STD_MUTEX_HPP

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

class std_mutex
  : private noncopyable
{
public:
  typedef netio::detail::scoped_lock<std_mutex> scoped_lock;

  // Constructor.
  std_mutex()
  {
  }

  // Destructor.
  ~std_mutex()
  {
  }

  // Try to lock the mutex.
  bool try_lock()
  {
    return mutex_.try_lock();
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

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_STD_MUTEX_HPP
