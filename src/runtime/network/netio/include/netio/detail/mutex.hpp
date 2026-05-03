#ifndef NETIO_DETAIL_MUTEX_HPP
#define NETIO_DETAIL_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_mutex.hpp"
#elif defined(NETIO_WINDOWS)
# include "netio/detail/win_mutex.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_mutex.hpp"
#else
# include "netio/detail/std_mutex.hpp"
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS)
typedef null_mutex mutex;
#elif defined(NETIO_WINDOWS)
typedef win_mutex mutex;
#elif defined(NETIO_HAS_PTHREADS)
typedef posix_mutex mutex;
#else
typedef std_mutex mutex;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_MUTEX_HPP
