#ifndef NETIO_DETAIL_FD_SET_ADAPTER_HPP
#define NETIO_DETAIL_FD_SET_ADAPTER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS_RUNTIME)

#include "netio/detail/posix_fd_set_adapter.hpp"
#include "netio/detail/win_fd_set_adapter.hpp"

namespace netio {
namespace detail {

#if defined(NETIO_WINDOWS) || defined(__CYGWIN__)
typedef win_fd_set_adapter fd_set_adapter;
#else
typedef posix_fd_set_adapter fd_set_adapter;
#endif

} // namespace detail
} // namespace netio

#endif // !defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_FD_SET_ADAPTER_HPP
