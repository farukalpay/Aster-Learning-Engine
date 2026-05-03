#ifndef NETIO_DETAIL_TIMER_SCHEDULER_HPP
#define NETIO_DETAIL_TIMER_SCHEDULER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/timer_scheduler_fwd.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)
# include "netio/detail/winrt_timer_scheduler.hpp"
#elif defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#elif defined(NETIO_HAS_IO_URING_AS_DEFAULT)
# include "netio/detail/io_uring_service.hpp"
#elif defined(NETIO_HAS_EPOLL)
# include "netio/detail/epoll_reactor.hpp"
#elif defined(NETIO_HAS_KQUEUE)
# include "netio/detail/kqueue_reactor.hpp"
#elif defined(NETIO_HAS_DEV_POLL)
# include "netio/detail/dev_poll_reactor.hpp"
#else
# include "netio/detail/select_reactor.hpp"
#endif

#endif // NETIO_DETAIL_TIMER_SCHEDULER_HPP
