#ifndef NETIO_DETAIL_STATIC_MUTEX_HPP
#define NETIO_DETAIL_STATIC_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_static_mutex.hpp"
#elif defined(NETIO_WINDOWS)
# include "netio/detail/win_static_mutex.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_static_mutex.hpp"
#else
# include "netio/detail/std_static_mutex.hpp"
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS)
typedef null_static_mutex static_mutex;
# define NETIO_STATIC_MUTEX_INIT NETIO_NULL_STATIC_MUTEX_INIT
#elif defined(NETIO_WINDOWS)
typedef win_static_mutex static_mutex;
# define NETIO_STATIC_MUTEX_INIT NETIO_WIN_STATIC_MUTEX_INIT
#elif defined(NETIO_HAS_PTHREADS)
typedef posix_static_mutex static_mutex;
# define NETIO_STATIC_MUTEX_INIT NETIO_POSIX_STATIC_MUTEX_INIT
#else
typedef std_static_mutex static_mutex;
# define NETIO_STATIC_MUTEX_INIT NETIO_STD_STATIC_MUTEX_INIT
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_STATIC_MUTEX_HPP
