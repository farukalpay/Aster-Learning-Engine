#ifndef NETIO_POSIX_DESCRIPTOR_HPP
#define NETIO_POSIX_DESCRIPTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_POSIX_STREAM_DESCRIPTOR) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/posix/basic_descriptor.hpp"

namespace netio {
namespace posix {

/// Typedef for the typical usage of basic_descriptor.
typedef basic_descriptor<> descriptor;

} // namespace posix
} // namespace netio

#endif // defined(NETIO_HAS_POSIX_STREAM_DESCRIPTOR)
       // || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_POSIX_DESCRIPTOR_HPP
