#ifndef NETIO_DETAIL_OPERATION_HPP
#define NETIO_DETAIL_OPERATION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_operation.hpp"
#else
# include "netio/detail/scheduler_operation.hpp"
#endif

namespace netio {
namespace detail {

#if defined(NETIO_HAS_IOCP)
typedef win_iocp_operation operation;
#else
typedef scheduler_operation operation;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_OPERATION_HPP
