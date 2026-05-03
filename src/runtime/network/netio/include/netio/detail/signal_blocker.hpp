#ifndef NETIO_DETAIL_SIGNAL_BLOCKER_HPP
#define NETIO_DETAIL_SIGNAL_BLOCKER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS) || defined(NETIO_WINDOWS) \
  || defined(NETIO_WINDOWS_RUNTIME) \
  || defined(__CYGWIN__) || defined(__SYMBIAN32__)
# include "netio/detail/null_signal_blocker.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_signal_blocker.hpp"
#else
# error Only Windows and POSIX are supported!
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS) || defined(NETIO_WINDOWS) \
  || defined(NETIO_WINDOWS_RUNTIME) \
  || defined(__CYGWIN__) || defined(__SYMBIAN32__)
typedef null_signal_blocker signal_blocker;
#elif defined(NETIO_HAS_PTHREADS)
typedef posix_signal_blocker signal_blocker;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_SIGNAL_BLOCKER_HPP
