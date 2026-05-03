#ifndef NETIO_DETAIL_GLOBAL_HPP
#define NETIO_DETAIL_GLOBAL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_global.hpp"
#elif defined(NETIO_WINDOWS)
# include "netio/detail/win_global.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_global.hpp"
#else
# include "netio/detail/std_global.hpp"
#endif

namespace netio {
namespace detail {

template <typename T>
inline T& global()
{
#if !defined(NETIO_HAS_THREADS)
  return null_global<T>();
#elif defined(NETIO_WINDOWS)
  return win_global<T>();
#elif defined(NETIO_HAS_PTHREADS)
  return posix_global<T>();
#else
  return std_global<T>();
#endif
}

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_GLOBAL_HPP
