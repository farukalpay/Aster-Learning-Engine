#ifndef NETIO_WINDOWS_STREAM_HANDLE_HPP
#define NETIO_WINDOWS_STREAM_HANDLE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_WINDOWS_STREAM_HANDLE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/windows/basic_stream_handle.hpp"

namespace netio {
namespace windows {

/// Typedef for the typical usage of a stream-oriented handle.
typedef basic_stream_handle<> stream_handle;

} // namespace windows
} // namespace netio

#endif // defined(NETIO_HAS_WINDOWS_STREAM_HANDLE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_WINDOWS_STREAM_HANDLE_HPP
