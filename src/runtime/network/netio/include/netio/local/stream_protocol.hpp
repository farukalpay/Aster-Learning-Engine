#ifndef NETIO_LOCAL_STREAM_PROTOCOL_HPP
#define NETIO_LOCAL_STREAM_PROTOCOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_LOCAL_SOCKETS) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_socket_acceptor.hpp"
#include "netio/basic_socket_iostream.hpp"
#include "netio/basic_stream_socket.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/local/basic_endpoint.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace local {

/// Encapsulates the flags needed for stream-oriented UNIX sockets.
/**
 * The netio::local::stream_protocol class contains flags necessary for
 * stream-oriented UNIX domain sockets.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 *
 * @par Concepts:
 * Protocol.
 */
class stream_protocol
{
public:
  /// Obtain an identifier for the type of the protocol.
  int type() const noexcept
  {
    return SOCK_STREAM;
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
  typedef basic_endpoint<stream_protocol> endpoint;

  /// The UNIX domain socket type.
  typedef basic_stream_socket<stream_protocol> socket;

  /// The UNIX domain acceptor type.
  typedef basic_socket_acceptor<stream_protocol> acceptor;

#if !defined(NETIO_NO_IOSTREAM)
  /// The UNIX domain iostream type.
  typedef basic_socket_iostream<stream_protocol> iostream;
#endif // !defined(NETIO_NO_IOSTREAM)
};

} // namespace local
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_LOCAL_SOCKETS)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_LOCAL_STREAM_PROTOCOL_HPP
