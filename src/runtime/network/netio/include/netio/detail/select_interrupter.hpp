#ifndef NETIO_DETAIL_SELECT_INTERRUPTER_HPP
#define NETIO_DETAIL_SELECT_INTERRUPTER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS_RUNTIME)

#if defined(NETIO_WINDOWS) || defined(__CYGWIN__) || defined(__SYMBIAN32__)
# include "netio/detail/socket_select_interrupter.hpp"
#elif defined(NETIO_HAS_EVENTFD)
# include "netio/detail/eventfd_select_interrupter.hpp"
#else
# include "netio/detail/pipe_select_interrupter.hpp"
#endif

namespace netio {
namespace detail {

#if defined(NETIO_WINDOWS) || defined(__CYGWIN__) || defined(__SYMBIAN32__)
typedef socket_select_interrupter select_interrupter;
#elif defined(NETIO_HAS_EVENTFD)
typedef eventfd_select_interrupter select_interrupter;
#else
typedef pipe_select_interrupter select_interrupter;
#endif

} // namespace detail
} // namespace netio

#endif // !defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_SELECT_INTERRUPTER_HPP
