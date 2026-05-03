#ifndef NETIO_DETAIL_NULL_SIGNAL_BLOCKER_HPP
#define NETIO_DETAIL_NULL_SIGNAL_BLOCKER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS) \
  || defined(NETIO_WINDOWS) \
  || defined(NETIO_WINDOWS_RUNTIME) \
  || defined(__CYGWIN__) \
  || defined(__SYMBIAN32__)

#include "netio/detail/noncopyable.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class null_signal_blocker
  : private noncopyable
{
public:
  // Constructor blocks all signals for the calling thread.
  null_signal_blocker()
  {
  }

  // Destructor restores the previous signal mask.
  ~null_signal_blocker()
  {
  }

  // Block all signals for the calling thread.
  void block()
  {
  }

  // Restore the previous signal mask.
  void unblock()
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_HAS_THREADS)
       // || defined(NETIO_WINDOWS)
       // || defined(NETIO_WINDOWS_RUNTIME)
       // || defined(__CYGWIN__)
       // || defined(__SYMBIAN32__)

#endif // NETIO_DETAIL_NULL_SIGNAL_BLOCKER_HPP
