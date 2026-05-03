#ifndef NETIO_DETAIL_WIN_MUTEX_HPP
#define NETIO_DETAIL_WIN_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include "netio/detail/noncopyable.hpp"
#include "netio/detail/scoped_lock.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class win_mutex
  : private noncopyable
{
public:
  typedef netio::detail::scoped_lock<win_mutex> scoped_lock;

  // Constructor.
  NETIO_DECL win_mutex();

  // Destructor.
  ~win_mutex()
  {
    ::DeleteCriticalSection(&crit_section_);
  }

  // Try to lock the mutex.
  bool try_lock()
  {
    return ::TryEnterCriticalSection(&crit_section_) != 0;
  }

  // Lock the mutex.
  void lock()
  {
    ::EnterCriticalSection(&crit_section_);
  }

  // Unlock the mutex.
  void unlock()
  {
    ::LeaveCriticalSection(&crit_section_);
  }

private:
  // Initialisation must be performed in a separate function to the constructor
  // since the compiler does not support the use of structured exceptions and
  // C++ exceptions in the same function.
  NETIO_DECL int do_init();

  ::CRITICAL_SECTION crit_section_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/win_mutex.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_WIN_MUTEX_HPP
