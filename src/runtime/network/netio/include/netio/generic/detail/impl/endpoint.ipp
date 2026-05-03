#ifndef NETIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP
#define NETIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include <cstring>
#include <typeinfo>
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/detail/throw_exception.hpp"
#include "netio/error.hpp"
#include "netio/generic/detail/endpoint.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace generic {
namespace detail {

endpoint::endpoint()
{
  init(0, 0, 0);
}

endpoint::endpoint(const void* sock_addr,
    std::size_t sock_addr_size, int sock_protocol)
{
  init(sock_addr, sock_addr_size, sock_protocol);
}

void endpoint::resize(std::size_t new_size)
{
  if (new_size > sizeof(netio::detail::sockaddr_storage_type))
  {
    netio::error_code ec(netio::error::invalid_argument);
    netio::detail::throw_error(ec);
  }
  else
  {
    size_ = new_size;
    protocol_ = 0;
  }
}

bool operator==(const endpoint& e1, const endpoint& e2)
{
  using namespace std; // For memcmp.
  return e1.size() == e2.size() && memcmp(e1.data(), e2.data(), e1.size()) == 0;
}

bool operator<(const endpoint& e1, const endpoint& e2)
{
  if (e1.protocol() < e2.protocol())
    return true;

  if (e1.protocol() > e2.protocol())
    return false;

  using namespace std; // For memcmp.
  std::size_t compare_size = e1.size() < e2.size() ? e1.size() : e2.size();
  int compare_result = memcmp(e1.data(), e2.data(), compare_size);

  if (compare_result < 0)
    return true;

  if (compare_result > 0)
    return false;

  return e1.size() < e2.size();
}

void endpoint::init(const void* sock_addr,
    std::size_t sock_addr_size, int sock_protocol)
{
  if (sock_addr_size > sizeof(netio::detail::sockaddr_storage_type))
  {
    netio::error_code ec(netio::error::invalid_argument);
    netio::detail::throw_error(ec);
  }

  using namespace std; // For memset and memcpy.
  memset(&data_.generic, 0, sizeof(netio::detail::sockaddr_storage_type));
  if (sock_addr_size > 0)
    memcpy(&data_.generic, sock_addr, sock_addr_size);

  size_ = sock_addr_size;
  protocol_ = sock_protocol;
}

} // namespace detail
} // namespace generic
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_GENERIC_DETAIL_IMPL_ENDPOINT_IPP
