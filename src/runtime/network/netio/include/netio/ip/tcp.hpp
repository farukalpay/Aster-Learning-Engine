#ifndef NETIO_IP_TCP_HPP
#define NETIO_IP_TCP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/basic_socket_acceptor.hpp"
#include "netio/basic_socket_iostream.hpp"
#include "netio/basic_stream_socket.hpp"
#include "netio/detail/socket_option.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/ip/basic_endpoint.hpp"
#include "netio/ip/basic_resolver.hpp"
#include "netio/ip/basic_resolver_iterator.hpp"
#include "netio/ip/basic_resolver_query.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// Encapsulates the flags needed for TCP.
/**
 * The netio::ip::tcp class contains flags necessary for TCP sockets.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 *
 * @par Concepts:
 * Protocol, InternetProtocol.
 */
class tcp
{
public:
  /// The type of a TCP endpoint.
  typedef basic_endpoint<tcp> endpoint;

  /// Construct to represent the IPv4 TCP protocol.
  static tcp v4() noexcept
  {
    return tcp(NETIO_OS_DEF(AF_INET));
  }

  /// Construct to represent the IPv6 TCP protocol.
  static tcp v6() noexcept
  {
    return tcp(NETIO_OS_DEF(AF_INET6));
  }

  /// Obtain an identifier for the type of the protocol.
  int type() const noexcept
  {
    return NETIO_OS_DEF(SOCK_STREAM);
  }

  /// Obtain an identifier for the protocol.
  int protocol() const noexcept
  {
    return NETIO_OS_DEF(IPPROTO_TCP);
  }

  /// Obtain an identifier for the protocol family.
  int family() const noexcept
  {
    return family_;
  }

  /// The TCP socket type.
  typedef basic_stream_socket<tcp> socket;

  /// The TCP acceptor type.
  typedef basic_socket_acceptor<tcp> acceptor;

  /// The TCP resolver type.
  typedef basic_resolver<tcp> resolver;

#if !defined(NETIO_NO_IOSTREAM)
  /// The TCP iostream type.
  typedef basic_socket_iostream<tcp> iostream;
#endif // !defined(NETIO_NO_IOSTREAM)

  /// Socket option for disabling the Nagle algorithm.
  /**
   * Implements the IPPROTO_TCP/TCP_NODELAY socket option.
   *
   * @par Examples
   * Setting the option:
   * @code
   * netio::ip::tcp::socket socket(my_context);
   * ...
   * netio::ip::tcp::no_delay option(true);
   * socket.set_option(option);
   * @endcode
   *
   * @par
   * Getting the current option value:
   * @code
   * netio::ip::tcp::socket socket(my_context);
   * ...
   * netio::ip::tcp::no_delay option;
   * socket.get_option(option);
   * bool is_set = option.value();
   * @endcode
   *
   * @par Concepts:
   * Socket_Option, Boolean_Socket_Option.
   */
#if defined(GENERATING_DOCUMENTATION)
  typedef implementation_defined no_delay;
#else
  typedef netio::detail::socket_option::boolean<
    NETIO_OS_DEF(IPPROTO_TCP), NETIO_OS_DEF(TCP_NODELAY)> no_delay;
#endif

  /// Compare two protocols for equality.
  friend bool operator==(const tcp& p1, const tcp& p2)
  {
    return p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const tcp& p1, const tcp& p2)
  {
    return p1.family_ != p2.family_;
  }

private:
  // Construct with a specific family.
  explicit tcp(int protocol_family) noexcept
    : family_(protocol_family)
  {
  }

  int family_;
};

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_TCP_HPP
