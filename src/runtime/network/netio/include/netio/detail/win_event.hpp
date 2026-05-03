#ifndef NETIO_DETAIL_WIN_EVENT_HPP
#define NETIO_DETAIL_WIN_EVENT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include <cstddef>
#include "netio/detail/assert.hpp"
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class win_event
  : private noncopyable
{
public:
  // Constructor.
  NETIO_DECL win_event();

  // Destructor.
  NETIO_DECL ~win_event();

  // Signal the event. (Retained for backward compatibility.)
  template <typename Lock>
  void signal(Lock& lock)
  {
    this->signal_all(lock);
  }

  // Signal all waiters.
  template <typename Lock>
  void signal_all(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    (void)lock;
    state_ |= 1;
    ::SetEvent(events_[0]);
  }

  // Unlock the mutex and signal one waiter.
  template <typename Lock>
  void unlock_and_signal_one(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    state_ |= 1;
    bool have_waiters = (state_ > 1);
    lock.unlock();
    if (have_waiters)
      ::SetEvent(events_[1]);
  }

  // Unlock the mutex and signal one waiter who may destroy us.
  template <typename Lock>
  void unlock_and_signal_one_for_destruction(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    state_ |= 1;
    bool have_waiters = (state_ > 1);
    if (have_waiters)
      ::SetEvent(events_[1]);
    lock.unlock();
  }

  // If there's a waiter, unlock the mutex and signal it.
  template <typename Lock>
  bool maybe_unlock_and_signal_one(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    state_ |= 1;
    if (state_ > 1)
    {
      lock.unlock();
      ::SetEvent(events_[1]);
      return true;
    }
    return false;
  }

  // Reset the event.
  template <typename Lock>
  void clear(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    (void)lock;
    ::ResetEvent(events_[0]);
    state_ &= ~std::size_t(1);
  }

  // Wait for the event to become signalled.
  template <typename Lock>
  void wait(Lock& lock)
  {
    NETIO_ASSERT(lock.locked());
    while ((state_ & 1) == 0)
    {
      state_ += 2;
      lock.unlock();
#if defined(NETIO_WINDOWS_APP)
      ::WaitForMultipleObjectsEx(2, events_, false, INFINITE, false);
#else // defined(NETIO_WINDOWS_APP)
      ::WaitForMultipleObjects(2, events_, false, INFINITE);
#endif // defined(NETIO_WINDOWS_APP)
      lock.lock();
      state_ -= 2;
    }
  }

  // Timed wait for the event to become signalled.
  template <typename Lock>
  bool wait_for_usec(Lock& lock, long usec)
  {
    NETIO_ASSERT(lock.locked());
    if ((state_ & 1) == 0)
    {
      state_ += 2;
      lock.unlock();
      DWORD msec = usec > 0 ? (usec < 1000 ? 1 : usec / 1000) : 0;
#if defined(NETIO_WINDOWS_APP)
      ::WaitForMultipleObjectsEx(2, events_, false, msec, false);
#else // defined(NETIO_WINDOWS_APP)
      ::WaitForMultipleObjects(2, events_, false, msec);
#endif // defined(NETIO_WINDOWS_APP)
      lock.lock();
      state_ -= 2;
    }
    return (state_ & 1) != 0;
  }

private:
  HANDLE events_[2];
  std::size_t state_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/win_event.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_WIN_EVENT_HPP
