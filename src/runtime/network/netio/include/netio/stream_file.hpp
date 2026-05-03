#ifndef NETIO_STREAM_FILE_HPP
#define NETIO_STREAM_FILE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_FILE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_stream_file.hpp"

namespace netio {

/// Typedef for the typical usage of a stream-oriented file.
typedef basic_stream_file<> stream_file;

} // namespace netio

#endif // defined(NETIO_HAS_FILE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_STREAM_FILE_HPP
