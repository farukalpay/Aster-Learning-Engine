#ifndef NETIO_READABLE_PIPE_HPP
#define NETIO_READABLE_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PIPE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_readable_pipe.hpp"

namespace netio {

/// Typedef for the typical usage of a readable pipe.
typedef basic_readable_pipe<> readable_pipe;

} // namespace netio

#endif // defined(NETIO_HAS_PIPE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_READABLE_PIPE_HPP
