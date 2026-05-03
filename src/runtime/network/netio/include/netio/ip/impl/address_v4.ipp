#ifndef NETIO_IP_IMPL_ADDRESS_V4_IPP
#define NETIO_IP_IMPL_ADDRESS_V4_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <climits>
#include <limits>
#include <stdexcept>
#include "netio/error.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/detail/throw_exception.hpp"
#include "netio/ip/address_v4.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

address_v4::address_v4(const address_v4::bytes_type& bytes)
{
#if UCHAR_MAX > 0xFF
  if (bytes[0] > 0xFF || bytes[1] > 0xFF
      || bytes[2] > 0xFF || bytes[3] > 0xFF)
  {
    std::out_of_range ex("address_v4 from bytes_type");
    netio::detail::throw_exception(ex);
  }
#endif // UCHAR_MAX > 0xFF

  using namespace std; // For memcpy.
  memcpy(&addr_.s_addr, bytes.data(), 4);
}

address_v4::address_v4(address_v4::uint_type addr)
{
  if ((std::numeric_limits<uint_type>::max)() > 0xFFFFFFFF)
  {
    std::out_of_range ex("address_v4 from unsigned integer");
    netio::detail::throw_exception(ex);
  }

  addr_.s_addr = netio::detail::socket_ops::host_to_network_long(
      static_cast<netio::detail::u_long_type>(addr));
}

address_v4::bytes_type address_v4::to_bytes() const noexcept
{
  using namespace std; // For memcpy.
  bytes_type bytes;
  memcpy(bytes.data(), &addr_.s_addr, 4);
  return bytes;
}

address_v4::uint_type address_v4::to_uint() const noexcept
{
  return netio::detail::socket_ops::network_to_host_long(addr_.s_addr);
}

std::string address_v4::to_string() const
{
  netio::error_code ec;
  char addr_str[netio::detail::max_addr_v4_str_len];
  const char* addr =
    netio::detail::socket_ops::inet_ntop(
        NETIO_OS_DEF(AF_INET), &addr_, addr_str,
        netio::detail::max_addr_v4_str_len, 0, ec);
  if (addr == 0)
    netio::detail::throw_error(ec);
  return addr;
}

bool address_v4::is_loopback() const noexcept
{
  return (to_uint() & 0xFF000000) == 0x7F000000;
}

bool address_v4::is_unspecified() const noexcept
{
  return to_uint() == 0;
}

bool address_v4::is_multicast() const noexcept
{
  return (to_uint() & 0xF0000000) == 0xE0000000;
}

address_v4 make_address_v4(const char* str)
{
  netio::error_code ec;
  address_v4 addr = make_address_v4(str, ec);
  netio::detail::throw_error(ec);
  return addr;
}

address_v4 make_address_v4(const char* str,
    netio::error_code& ec) noexcept
{
  address_v4::bytes_type bytes;
  if (netio::detail::socket_ops::inet_pton(
        NETIO_OS_DEF(AF_INET), str, &bytes, 0, ec) <= 0)
    return address_v4();
  return address_v4(bytes);
}

address_v4 make_address_v4(const std::string& str)
{
  return make_address_v4(str.c_str());
}

address_v4 make_address_v4(const std::string& str,
    netio::error_code& ec) noexcept
{
  return make_address_v4(str.c_str(), ec);
}

#if defined(NETIO_HAS_STRING_VIEW)

address_v4 make_address_v4(string_view str)
{
  return make_address_v4(static_cast<std::string>(str));
}

address_v4 make_address_v4(string_view str,
    netio::error_code& ec) noexcept
{
  return make_address_v4(static_cast<std::string>(str), ec);
}

#endif // defined(NETIO_HAS_STRING_VIEW)

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_IMPL_ADDRESS_V4_IPP
