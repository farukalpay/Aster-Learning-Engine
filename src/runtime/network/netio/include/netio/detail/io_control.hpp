#ifndef NETIO_DETAIL_IO_CONTROL_HPP
#define NETIO_DETAIL_IO_CONTROL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <cstddef>
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {
namespace io_control {

// I/O control command for getting number of bytes available.
class bytes_readable
{
public:
  // Default constructor.
  bytes_readable()
    : value_(0)
  {
  }

  // Construct with a specific command value.
  bytes_readable(std::size_t value)
    : value_(static_cast<detail::ioctl_arg_type>(value))
  {
  }

  // Get the name of the IO control command.
  int name() const
  {
    return static_cast<int>(NETIO_OS_DEF(FIONREAD));
  }

  // Set the value of the I/O control command.
  void set(std::size_t value)
  {
    value_ = static_cast<detail::ioctl_arg_type>(value);
  }

  // Get the current value of the I/O control command.
  std::size_t get() const
  {
    return static_cast<std::size_t>(value_);
  }

  // Get the address of the command data.
  detail::ioctl_arg_type* data()
  {
    return &value_;
  }

  // Get the address of the command data.
  const detail::ioctl_arg_type* data() const
  {
    return &value_;
  }

private:
  detail::ioctl_arg_type value_;
};

} // namespace io_control
} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_IO_CONTROL_HPP
