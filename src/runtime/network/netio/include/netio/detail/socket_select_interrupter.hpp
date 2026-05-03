#ifndef NETIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP
#define NETIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS_RUNTIME)

#if defined(NETIO_WINDOWS) \
  || defined(__CYGWIN__) \
  || defined(__SYMBIAN32__)

#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class socket_select_interrupter
{
public:
  // Constructor.
  NETIO_DECL explicit socket_select_interrupter(bool = true);

  // Destructor.
  NETIO_DECL ~socket_select_interrupter();

  // Recreate the interrupter's descriptors. Used after a fork.
  NETIO_DECL void recreate();

  // Interrupt the select call.
  NETIO_DECL void interrupt();

  // Reset the select interrupter. Returns true if the reset was successful.
  NETIO_DECL bool reset();

  // Get the read descriptor to be passed to select.
  socket_type read_descriptor() const
  {
    return read_descriptor_;
  }

private:
  // Open the descriptors. Throws on error.
  NETIO_DECL void open_descriptors();

  // Close the descriptors.
  NETIO_DECL void close_descriptors();

  // The read end of a connection used to interrupt the select call. This file
  // descriptor is passed to select such that when it is time to stop, a single
  // byte will be written on the other end of the connection and this
  // descriptor will become readable.
  socket_type read_descriptor_;

  // The write end of a connection used to interrupt the select call. A single
  // byte may be written to this to wake up the select which is waiting for the
  // other end to become readable.
  socket_type write_descriptor_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/socket_select_interrupter.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS)
       // || defined(__CYGWIN__)
       // || defined(__SYMBIAN32__)

#endif // !defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_SOCKET_SELECT_INTERRUPTER_HPP
