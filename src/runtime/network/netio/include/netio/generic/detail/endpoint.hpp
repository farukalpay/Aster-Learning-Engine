#ifndef NETIO_GENERIC_DETAIL_ENDPOINT_HPP
#define NETIO_GENERIC_DETAIL_ENDPOINT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include <cstddef>
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace generic {
namespace detail {

// Helper class for implementing a generic socket endpoint.
class endpoint
{
public:
  // Default constructor.
  NETIO_DECL endpoint();

  // Construct an endpoint from the specified raw bytes.
  NETIO_DECL endpoint(const void* sock_addr,
      std::size_t sock_addr_size, int sock_protocol);

  // Copy constructor.
  endpoint(const endpoint& other)
    : data_(other.data_),
      size_(other.size_),
      protocol_(other.protocol_)
  {
  }

  // Assign from another endpoint.
  endpoint& operator=(const endpoint& other)
  {
    data_ = other.data_;
    size_ = other.size_;
    protocol_ = other.protocol_;
    return *this;
  }

  // Get the address family associated with the endpoint.
  int family() const
  {
    return data_.base.sa_family;
  }

  // Get the socket protocol associated with the endpoint.
  int protocol() const
  {
    return protocol_;
  }

  // Get the underlying endpoint in the native type.
  netio::detail::socket_addr_type* data()
  {
    return &data_.base;
  }

  // Get the underlying endpoint in the native type.
  const netio::detail::socket_addr_type* data() const
  {
    return &data_.base;
  }

  // Get the underlying size of the endpoint in the native type.
  std::size_t size() const
  {
    return size_;
  }

  // Set the underlying size of the endpoint in the native type.
  NETIO_DECL void resize(std::size_t size);

  // Get the capacity of the endpoint in the native type.
  std::size_t capacity() const
  {
    return sizeof(netio::detail::sockaddr_storage_type);
  }

  // Compare two endpoints for equality.
  NETIO_DECL friend bool operator==(
      const endpoint& e1, const endpoint& e2);

  // Compare endpoints for ordering.
  NETIO_DECL friend bool operator<(
      const endpoint& e1, const endpoint& e2);

private:
  // The underlying socket address.
  union data_union
  {
    netio::detail::socket_addr_type base;
    netio::detail::sockaddr_storage_type generic;
  } data_;

  // The length of the socket address stored in the endpoint.
  std::size_t size_;

  // The socket protocol associated with the endpoint.
  int protocol_;

  // Initialise with a specified memory.
  NETIO_DECL void init(const void* sock_addr,
      std::size_t sock_addr_size, int sock_protocol);
};

} // namespace detail
} // namespace generic
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/generic/detail/impl/endpoint.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_GENERIC_DETAIL_ENDPOINT_HPP
