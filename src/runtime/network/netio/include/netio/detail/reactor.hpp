#ifndef NETIO_DETAIL_REACTOR_HPP
#define NETIO_DETAIL_REACTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP) || defined(NETIO_WINDOWS_RUNTIME)
# include "netio/detail/null_reactor.hpp"
#elif defined(NETIO_HAS_IO_URING_AS_DEFAULT)
# include "netio/detail/null_reactor.hpp"
#elif defined(NETIO_HAS_EPOLL)
# include "netio/detail/epoll_reactor.hpp"
#elif defined(NETIO_HAS_KQUEUE)
# include "netio/detail/kqueue_reactor.hpp"
#elif defined(NETIO_HAS_DEV_POLL)
# include "netio/detail/dev_poll_reactor.hpp"
#else
# include "netio/detail/select_reactor.hpp"
#endif

namespace netio {
namespace detail {

#if defined(NETIO_HAS_IOCP) || defined(NETIO_WINDOWS_RUNTIME)
typedef null_reactor reactor;
#elif defined(NETIO_HAS_IO_URING_AS_DEFAULT)
typedef null_reactor reactor;
#elif defined(NETIO_HAS_EPOLL)
typedef epoll_reactor reactor;
#elif defined(NETIO_HAS_KQUEUE)
typedef kqueue_reactor reactor;
#elif defined(NETIO_HAS_DEV_POLL)
typedef dev_poll_reactor reactor;
#else
typedef select_reactor reactor;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_REACTOR_HPP
