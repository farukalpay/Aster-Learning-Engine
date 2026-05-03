#ifndef NETIO_DETAIL_CONDITIONALLY_ENABLED_EVENT_HPP
#define NETIO_DETAIL_CONDITIONALLY_ENABLED_EVENT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/conditionally_enabled_mutex.hpp"
#include "netio/detail/event.hpp"
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/null_event.hpp"
#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

// Mutex adapter used to conditionally enable or disable locking.
class conditionally_enabled_event
  : private noncopyable
{
public:
  // Constructor.
  conditionally_enabled_event()
  {
  }

  // Destructor.
  ~conditionally_enabled_event()
  {
  }

  // Signal the event. (Retained for backward compatibility.)
  void signal(conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.signal(lock);
  }

  // Signal all waiters.
  void signal_all(conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.signal_all(lock);
  }

  // Unlock the mutex and signal one waiter.
  void unlock_and_signal_one(
      conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.unlock_and_signal_one(lock);
  }

  // Unlock the mutex and signal one waiter who may destroy us.
  void unlock_and_signal_one_for_destruction(
      conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.unlock_and_signal_one(lock);
  }

  // If there's a waiter, unlock the mutex and signal it.
  bool maybe_unlock_and_signal_one(
      conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      return event_.maybe_unlock_and_signal_one(lock);
    else
      return false;
  }

  // Reset the event.
  void clear(conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.clear(lock);
  }

  // Wait for the event to become signalled.
  void wait(conditionally_enabled_mutex::scoped_lock& lock)
  {
    if (lock.mutex_.enabled_)
      event_.wait(lock);
    else
      null_event().wait(lock);
  }

  // Timed wait for the event to become signalled.
  bool wait_for_usec(
      conditionally_enabled_mutex::scoped_lock& lock, long usec)
  {
    if (lock.mutex_.enabled_)
      return event_.wait_for_usec(lock, usec);
    else
      return null_event().wait_for_usec(lock, usec);
  }

private:
  netio::detail::event event_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_CONDITIONALLY_ENABLED_EVENT_HPP
