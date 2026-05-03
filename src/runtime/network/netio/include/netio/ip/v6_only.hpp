#ifndef NETIO_IP_V6_ONLY_HPP
#define NETIO_IP_V6_ONLY_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/socket_option.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// Socket option for determining whether an IPv6 socket supports IPv6
/// communication only.
/**
 * Implements the IPPROTO_IPV6/IPV6_V6ONLY socket option.
 *
 * @par Examples
 * Setting the option:
 * @code
 * netio::ip::tcp::socket socket(my_context);
 * ...
 * netio::ip::v6_only option(true);
 * socket.set_option(option);
 * @endcode
 *
 * @par
 * Getting the current option value:
 * @code
 * netio::ip::tcp::socket socket(my_context);
 * ...
 * netio::ip::v6_only option;
 * socket.get_option(option);
 * bool v6_only = option.value();
 * @endcode
 *
 * @par Concepts:
 * GettableSocketOption, SettableSocketOption.
 */
#if defined(GENERATING_DOCUMENTATION)
typedef implementation_defined v6_only;
#elif defined(IPV6_V6ONLY)
typedef netio::detail::socket_option::boolean<
    IPPROTO_IPV6, IPV6_V6ONLY> v6_only;
#else
typedef netio::detail::socket_option::boolean<
    netio::detail::custom_socket_option_level,
    netio::detail::always_fail_option> v6_only;
#endif

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_V6_ONLY_HPP
