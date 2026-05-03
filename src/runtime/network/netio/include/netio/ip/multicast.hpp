#ifndef NETIO_IP_MULTICAST_HPP
#define NETIO_IP_MULTICAST_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <cstddef>
#include "netio/ip/detail/socket_option.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {
namespace multicast {

/// Socket option to join a multicast group on a specified interface.
/**
 * Implements the IPPROTO_IP/IP_ADD_MEMBERSHIP socket option.
 *
 * @par Examples
 * Setting the option to join a multicast group:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::address multicast_address =
 *   netio::ip::address::from_string("225.0.0.1");
 * netio::ip::multicast::join_group option(multicast_address);
 * socket.set_option(option);
 * @endcode
 *
 * @par Concepts:
 * SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined join_group;
#else
typedef netio::ip::detail::socket_option::multicast_request<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_ADD_MEMBERSHIP),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_JOIN_GROUP)> join_group;
#endif

/// Socket option to leave a multicast group on a specified interface.
/**
 * Implements the IPPROTO_IP/IP_DROP_MEMBERSHIP socket option.
 *
 * @par Examples
 * Setting the option to leave a multicast group:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::address multicast_address =
 *   netio::ip::address::from_string("225.0.0.1");
 * netio::ip::multicast::leave_group option(multicast_address);
 * socket.set_option(option);
 * @endcode
 *
 * @par Concepts:
 * SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined leave_group;
#else
typedef netio::ip::detail::socket_option::multicast_request<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_DROP_MEMBERSHIP),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_LEAVE_GROUP)> leave_group;
#endif

/// Socket option for local interface to use for outgoing multicast packets.
/**
 * Implements the IPPROTO_IP/IP_MULTICAST_IF socket option.
 *
 * @par Examples
 * Setting the option:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::address_v4 local_interface =
 *   netio::ip::address_v4::from_string("1.2.3.4");
 * netio::ip::multicast::outbound_interface option(local_interface);
 * socket.set_option(option);
 * @endcode
 *
 * @par Concepts:
 * SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined outbound_interface;
#else
typedef netio::ip::detail::socket_option::network_interface<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_MULTICAST_IF),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_MULTICAST_IF)> outbound_interface;
#endif

/// Socket option for time-to-live associated with outgoing multicast packets.
/**
 * Implements the IPPROTO_IP/IP_MULTICAST_TTL socket option.
 *
 * @par Examples
 * Setting the option:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::multicast::hops option(4);
 * socket.set_option(option);
 * @endcode
 *
 * @par
 * Getting the current option value:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::multicast::hops option;
 * socket.get_option(option);
 * int ttl = option.value();
 * @endcode
 *
 * @par Concepts:
 * GettableSocketOption, SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined hops;
#else
typedef netio::ip::detail::socket_option::multicast_hops<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_MULTICAST_TTL),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_MULTICAST_HOPS)> hops;
#endif

/// Socket option determining whether outgoing multicast packets will be
/// received on the same socket if it is a member of the multicast group.
/**
 * Implements the IPPROTO_IP/IP_MULTICAST_LOOP socket option.
 *
 * @par Examples
 * Setting the option:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::multicast::enable_loopback option(true);
 * socket.set_option(option);
 * @endcode
 *
 * @par
 * Getting the current option value:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::multicast::enable_loopback option;
 * socket.get_option(option);
 * bool is_set = option.value();
 * @endcode
 *
 * @par Concepts:
 * GettableSocketOption, SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined enable_loopback;
#else
typedef netio::ip::detail::socket_option::multicast_enable_loopback<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_MULTICAST_LOOP),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_MULTICAST_LOOP)> enable_loopback;
#endif

} // namespace multicast
} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_MULTICAST_HPP
