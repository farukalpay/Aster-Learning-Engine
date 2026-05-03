#ifndef NETIO_SERIAL_PORT_HPP
#define NETIO_SERIAL_PORT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_SERIAL_PORT) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_serial_port.hpp"

namespace netio {

/// Typedef for the typical usage of a serial port.
typedef basic_serial_port<> serial_port;

} // namespace netio

#endif // defined(NETIO_HAS_SERIAL_PORT)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_SERIAL_PORT_HPP
