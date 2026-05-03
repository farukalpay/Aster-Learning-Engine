#ifndef NETIO_IP_HOST_NAME_HPP
#define NETIO_IP_HOST_NAME_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <string>
#include "netio/error_code.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// Get the current host name.
NETIO_DECL std::string host_name();

/// Get the current host name.
NETIO_DECL std::string host_name(netio::error_code& ec);

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/ip/impl/host_name.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_IP_HOST_NAME_HPP
