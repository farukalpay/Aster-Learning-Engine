#ifndef NETIO_DETAIL_EVENTFD_SELECT_INTERRUPTER_HPP
#define NETIO_DETAIL_EVENTFD_SELECT_INTERRUPTER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_EVENTFD)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class eventfd_select_interrupter
{
public:
  // Constructor.
  NETIO_DECL explicit eventfd_select_interrupter(bool use_eventfd = true);

  // Destructor.
  NETIO_DECL ~eventfd_select_interrupter();

  // Recreate the interrupter's descriptors. Used after a fork.
  NETIO_DECL void recreate();

  // Interrupt the select call.
  NETIO_DECL void interrupt();

  // Reset the select interrupter. Returns true if the reset was successful.
  NETIO_DECL bool reset();

  // Get the read descriptor to be passed to select.
  int read_descriptor() const
  {
    return read_descriptor_;
  }

private:
  // Open the descriptors. Throws on error.
  NETIO_DECL void open_descriptors(bool use_eventfd);

  // Close the descriptors.
  NETIO_DECL void close_descriptors();

  // The read end of a connection used to interrupt the select call. This file
  // descriptor is passed to select such that when it is time to stop, a single
  // 64bit value will be written on the other end of the connection and this
  // descriptor will become readable.
  int read_descriptor_;

  // The write end of a connection used to interrupt the select call. A single
  // 64bit non-zero value may be written to this to wake up the select which is
  // waiting for the other end to become readable. This descriptor will only
  // differ from the read descriptor when a pipe is used.
  int write_descriptor_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/eventfd_select_interrupter.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_EVENTFD)

#endif // NETIO_DETAIL_EVENTFD_SELECT_INTERRUPTER_HPP
