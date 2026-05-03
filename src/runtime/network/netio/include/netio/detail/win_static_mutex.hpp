#ifndef NETIO_DETAIL_WIN_STATIC_MUTEX_HPP
#define NETIO_DETAIL_WIN_STATIC_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include "netio/detail/scoped_lock.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

struct win_static_mutex
{
  typedef netio::detail::scoped_lock<win_static_mutex> scoped_lock;

  // Initialise the mutex.
  NETIO_DECL void init();

  // Initialisation must be performed in a separate function to the "public"
  // init() function since the compiler does not support the use of structured
  // exceptions and C++ exceptions in the same function.
  NETIO_DECL int do_init();

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

  bool initialised_;
  ::CRITICAL_SECTION crit_section_;
};

#if defined(UNDER_CE)
# define NETIO_WIN_STATIC_MUTEX_INIT { false, { 0, 0, 0, 0, 0 } }
#else // defined(UNDER_CE)
# define NETIO_WIN_STATIC_MUTEX_INIT { false, { 0, 0, 0, 0, 0, 0 } }
#endif // defined(UNDER_CE)

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/win_static_mutex.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_WIN_STATIC_MUTEX_HPP
