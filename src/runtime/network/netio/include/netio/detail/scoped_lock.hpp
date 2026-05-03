#ifndef NETIO_DETAIL_SCOPED_LOCK_HPP
#define NETIO_DETAIL_SCOPED_LOCK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/noncopyable.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

// Helper class to lock and unlock a mutex automatically.
template <typename Mutex>
class scoped_lock
  : private noncopyable
{
public:
  // Tag type used to distinguish constructors.
  enum adopt_lock_t { adopt_lock };

  // Constructor adopts a lock that is already held.
  scoped_lock(Mutex& m, adopt_lock_t)
    : mutex_(m),
      locked_(true)
  {
  }

  // Constructor acquires the lock.
  explicit scoped_lock(Mutex& m)
    : mutex_(m)
  {
    mutex_.lock();
    locked_ = true;
  }

  // Destructor releases the lock.
  ~scoped_lock()
  {
    if (locked_)
      mutex_.unlock();
  }

  // Explicitly acquire the lock.
  void lock()
  {
    if (!locked_)
    {
      mutex_.lock();
      locked_ = true;
    }
  }

  // Explicitly release the lock.
  void unlock()
  {
    if (locked_)
    {
      mutex_.unlock();
      locked_ = false;
    }
  }

  // Test whether the lock is held.
  bool locked() const
  {
    return locked_;
  }

  // Get the underlying mutex.
  Mutex& mutex()
  {
    return mutex_;
  }

private:
  // The underlying mutex.
  Mutex& mutex_;

  // Whether the mutex is currently locked or unlocked.
  bool locked_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_SCOPED_LOCK_HPP
