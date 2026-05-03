#ifndef NETIO_DETAIL_EVENT_HPP
#define NETIO_DETAIL_EVENT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_event.hpp"
#elif defined(NETIO_WINDOWS)
# include "netio/detail/win_event.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_event.hpp"
#else
# include "netio/detail/std_event.hpp"
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS)
typedef null_event event;
#elif defined(NETIO_WINDOWS)
typedef win_event event;
#elif defined(NETIO_HAS_PTHREADS)
typedef posix_event event;
#else
typedef std_event event;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_EVENT_HPP
