#ifndef NETIO_IP_UDP_HPP
#define NETIO_IP_UDP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/basic_datagram_socket.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/ip/basic_endpoint.hpp"
#include "netio/ip/basic_resolver.hpp"
#include "netio/ip/basic_resolver_iterator.hpp"
#include "netio/ip/basic_resolver_query.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// Encapsulates the flags needed for UDP.
/**
 * The netio::ip::udp class contains flags necessary for UDP sockets.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 *
 * @par Concepts:
 * Protocol, InternetProtocol.
 */
class udp
{
public:
  /// The type of a UDP endpoint.
  typedef basic_endpoint<udp> endpoint;

  /// Construct to represent the IPv4 UDP protocol.
  static udp v4() noexcept
  {
    return udp(NETIO_OS_DEF(AF_INET));
  }

  /// Construct to represent the IPv6 UDP protocol.
  static udp v6() noexcept
  {
    return udp(NETIO_OS_DEF(AF_INET6));
  }

  /// Obtain an identifier for the type of the protocol.
  int type() const noexcept
  {
    return NETIO_OS_DEF(SOCK_DGRAM);
  }

  /// Obtain an identifier for the protocol.
  int protocol() const noexcept
  {
    return NETIO_OS_DEF(IPPROTO_UDP);
  }

  /// Obtain an identifier for the protocol family.
  int family() const noexcept
  {
    return family_;
  }

  /// The UDP socket type.
  typedef basic_datagram_socket<udp> socket;

  /// The UDP resolver type.
  typedef basic_resolver<udp> resolver;

  /// Compare two protocols for equality.
  friend bool operator==(const udp& p1, const udp& p2)
  {
    return p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const udp& p1, const udp& p2)
  {
    return p1.family_ != p2.family_;
  }

private:
  // Construct with a specific family.
  explicit udp(int protocol_family) noexcept
    : family_(protocol_family)
  {
  }

  int family_;
};

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_UDP_HPP
