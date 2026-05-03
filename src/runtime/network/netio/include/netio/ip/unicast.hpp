#ifndef NETIO_IP_UNICAST_HPP
#define NETIO_IP_UNICAST_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <cstddef>
#include "netio/ip/detail/socket_option.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {
namespace unicast {

/// Socket option for time-to-live associated with outgoing unicast packets.
/**
 * Implements the IPPROTO_IP/IP_UNICAST_TTL socket option.
 *
 * @par Examples
 * Setting the option:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::unicast::hops option(4);
 * socket.set_option(option);
 * @endcode
 *
 * @par
 * Getting the current option value:
 * @code
 * netio::ip::udp::socket socket(my_context);
 * ...
 * netio::ip::unicast::hops option;
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
typedef netio::ip::detail::socket_option::unicast_hops<
  NETIO_OS_DEF(IPPROTO_IP),
  NETIO_OS_DEF(IP_TTL),
  NETIO_OS_DEF(IPPROTO_IPV6),
  NETIO_OS_DEF(IPV6_UNICAST_HOPS)> hops;
#endif

} // namespace unicast
} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_UNICAST_HPP
