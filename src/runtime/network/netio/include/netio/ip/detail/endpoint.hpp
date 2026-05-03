#ifndef NETIO_IP_DETAIL_ENDPOINT_HPP
#define NETIO_IP_DETAIL_ENDPOINT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <string>
#include "netio/detail/socket_types.hpp"
#include "netio/detail/winsock_init.hpp"
#include "netio/error_code.hpp"
#include "netio/ip/address.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {
namespace detail {

// Helper class for implementing an IP endpoint.
class endpoint
{
public:
  // Default constructor.
  NETIO_DECL endpoint() noexcept;

  // Construct an endpoint using a family and port number.
  NETIO_DECL endpoint(int family,
      unsigned short port_num) noexcept;

  // Construct an endpoint using an address and port number.
  NETIO_DECL endpoint(const netio::ip::address& addr,
      unsigned short port_num) noexcept;

  // Copy constructor.
  endpoint(const endpoint& other) noexcept
    : data_(other.data_)
  {
  }

  // Assign from another endpoint.
  endpoint& operator=(const endpoint& other) noexcept
  {
    data_ = other.data_;
    return *this;
  }

  // Get the underlying endpoint in the native type.
  netio::detail::socket_addr_type* data() noexcept
  {
    return &data_.base;
  }

  // Get the underlying endpoint in the native type.
  const netio::detail::socket_addr_type* data() const noexcept
  {
    return &data_.base;
  }

  // Get the underlying size of the endpoint in the native type.
  std::size_t size() const noexcept
  {
    if (is_v4())
      return sizeof(netio::detail::sockaddr_in4_type);
    else
      return sizeof(netio::detail::sockaddr_in6_type);
  }

  // Set the underlying size of the endpoint in the native type.
  NETIO_DECL void resize(std::size_t new_size);

  // Get the capacity of the endpoint in the native type.
  std::size_t capacity() const noexcept
  {
    return sizeof(data_);
  }

  // Get the port associated with the endpoint.
  NETIO_DECL unsigned short port() const noexcept;

  // Set the port associated with the endpoint.
  NETIO_DECL void port(unsigned short port_num) noexcept;

  // Get the IP address associated with the endpoint.
  NETIO_DECL netio::ip::address address() const noexcept;

  // Set the IP address associated with the endpoint.
  NETIO_DECL void address(
      const netio::ip::address& addr) noexcept;

  // Compare two endpoints for equality.
  NETIO_DECL friend bool operator==(const endpoint& e1,
      const endpoint& e2) noexcept;

  // Compare endpoints for ordering.
  NETIO_DECL friend bool operator<(const endpoint& e1,
      const endpoint& e2) noexcept;

  // Determine whether the endpoint is IPv4.
  bool is_v4() const noexcept
  {
    return data_.base.sa_family == NETIO_OS_DEF(AF_INET);
  }

#if !defined(NETIO_NO_IOSTREAM)
  // Convert to a string.
  NETIO_DECL std::string to_string() const;
#endif // !defined(NETIO_NO_IOSTREAM)

private:
  // The underlying IP socket address.
  union data_union
  {
    netio::detail::socket_addr_type base;
    netio::detail::sockaddr_in4_type v4;
    netio::detail::sockaddr_in6_type v6;
  } data_;
};

} // namespace detail
} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/ip/detail/impl/endpoint.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_IP_DETAIL_ENDPOINT_HPP
