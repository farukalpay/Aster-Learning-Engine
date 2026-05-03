#ifndef NETIO_DETAIL_TIMER_SCHEDULER_FWD_HPP
#define NETIO_DETAIL_TIMER_SCHEDULER_FWD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

namespace netio {
namespace detail {

#if defined(NETIO_WINDOWS_RUNTIME)
typedef class winrt_timer_scheduler timer_scheduler;
#elif defined(NETIO_HAS_IOCP)
typedef class win_iocp_io_context timer_scheduler;
#elif defined(NETIO_HAS_IO_URING_AS_DEFAULT)
typedef class io_uring_service timer_scheduler;
#elif defined(NETIO_HAS_EPOLL)
typedef class epoll_reactor timer_scheduler;
#elif defined(NETIO_HAS_KQUEUE)
typedef class kqueue_reactor timer_scheduler;
#elif defined(NETIO_HAS_DEV_POLL)
typedef class dev_poll_reactor timer_scheduler;
#else
typedef class select_reactor timer_scheduler;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_TIMER_SCHEDULER_FWD_HPP
