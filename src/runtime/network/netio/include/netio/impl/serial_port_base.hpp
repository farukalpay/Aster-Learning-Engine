#ifndef NETIO_IMPL_SERIAL_PORT_BASE_HPP
#define NETIO_IMPL_SERIAL_PORT_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/push_options.hpp"

namespace netio {

inline serial_port_base::baud_rate::baud_rate(unsigned int rate)
  : value_(rate)
{
}

inline unsigned int serial_port_base::baud_rate::value() const
{
  return value_;
}

inline serial_port_base::flow_control::type
serial_port_base::flow_control::value() const
{
  return value_;
}

inline serial_port_base::parity::type serial_port_base::parity::value() const
{
  return value_;
}

inline serial_port_base::stop_bits::type
serial_port_base::stop_bits::value() const
{
  return value_;
}

inline unsigned int serial_port_base::character_size::value() const
{
  return value_;
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_SERIAL_PORT_BASE_HPP
