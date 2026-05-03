#ifndef NETIO_DETAIL_THREAD_HPP
#define NETIO_DETAIL_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_thread.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_thread.hpp"
#elif defined(NETIO_WINDOWS)
# if defined(UNDER_CE)
#  include "netio/detail/wince_thread.hpp"
# elif defined(NETIO_WINDOWS_APP)
#  include "netio/detail/winapp_thread.hpp"
# else
#  include "netio/detail/win_thread.hpp"
# endif
#else
# include "netio/detail/std_thread.hpp"
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS)
typedef null_thread thread;
#elif defined(NETIO_HAS_PTHREADS)
typedef posix_thread thread;
#elif defined(NETIO_WINDOWS)
# if defined(UNDER_CE)
typedef wince_thread thread;
# elif defined(NETIO_WINDOWS_APP)
typedef winapp_thread thread;
# else
typedef win_thread thread;
# endif
#else
typedef std_thread thread;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_THREAD_HPP
