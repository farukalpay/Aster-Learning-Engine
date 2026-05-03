#ifndef NETIO_DETAIL_WIN_IOCP_THREAD_INFO_HPP
#define NETIO_DETAIL_WIN_IOCP_THREAD_INFO_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/thread_info_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

struct win_iocp_thread_info : public thread_info_base
{
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_WIN_IOCP_THREAD_INFO_HPP
