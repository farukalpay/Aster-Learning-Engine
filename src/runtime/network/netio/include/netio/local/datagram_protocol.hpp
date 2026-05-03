#ifndef NETIO_LOCAL_DATAGRAM_PROTOCOL_HPP
#define NETIO_LOCAL_DATAGRAM_PROTOCOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_LOCAL_SOCKETS) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_datagram_socket.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/local/basic_endpoint.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace local {

/// Encapsulates the flags needed for datagram-oriented UNIX sockets.
/**
 * The netio::local::datagram_protocol class contains flags necessary for
 * datagram-oriented UNIX domain sockets.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 *
 * @par Concepts:
 * Protocol.
 */
class datagram_protocol
{
public:
  /// Obtain an identifier for the type of the protocol.
  int type() const noexcept
  {
    return SOCK_DGRAM;
  }

  /// Obtain an identifier for the protocol.
  int protocol() const noexcept
  {
    return 0;
  }

  /// Obtain an identifier for the protocol family.
  int family() const noexcept
  {
    return AF_UNIX;
  }

  /// The type of a UNIX domain endpoint.
  typedef basic_endpoint<datagram_protocol> endpoint;

  /// The UNIX domain socket type.
  typedef basic_datagram_socket<datagram_protocol> socket;
};

} // namespace local
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_LOCAL_SOCKETS)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_LOCAL_DATAGRAM_PROTOCOL_HPP
