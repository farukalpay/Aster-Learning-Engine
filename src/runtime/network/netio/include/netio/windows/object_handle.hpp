#ifndef NETIO_WINDOWS_OBJECT_HANDLE_HPP
#define NETIO_WINDOWS_OBJECT_HANDLE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_WINDOWS_OBJECT_HANDLE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/windows/basic_object_handle.hpp"

namespace netio {
namespace windows {

/// Typedef for the typical usage of an object handle.
typedef basic_object_handle<> object_handle;

} // namespace windows
} // namespace netio

#endif // defined(NETIO_HAS_WINDOWS_OBJECT_HANDLE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_WINDOWS_OBJECT_HANDLE_HPP
