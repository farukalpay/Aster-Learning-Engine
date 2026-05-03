#ifndef NETIO_IP_IMPL_HOST_NAME_IPP
#define NETIO_IP_IMPL_HOST_NAME_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/detail/winsock_init.hpp"
#include "netio/ip/host_name.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

std::string host_name()
{
  char name[1024];
  netio::error_code ec;
  if (netio::detail::socket_ops::gethostname(name, sizeof(name), ec) != 0)
  {
    netio::detail::throw_error(ec);
    return std::string();
  }
  return std::string(name);
}

std::string host_name(netio::error_code& ec)
{
  char name[1024];
  if (netio::detail::socket_ops::gethostname(name, sizeof(name), ec) != 0)
    return std::string();
  return std::string(name);
}

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_IMPL_HOST_NAME_IPP
