#ifndef NETIO_WRITABLE_PIPE_HPP
#define NETIO_WRITABLE_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PIPE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_writable_pipe.hpp"

namespace netio {

/// Typedef for the typical usage of a writable pipe.
typedef basic_writable_pipe<> writable_pipe;

} // namespace netio

#endif // defined(NETIO_HAS_PIPE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_WRITABLE_PIPE_HPP
