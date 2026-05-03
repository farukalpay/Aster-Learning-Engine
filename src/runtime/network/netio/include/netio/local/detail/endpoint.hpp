#ifndef NETIO_LOCAL_DETAIL_ENDPOINT_HPP
#define NETIO_LOCAL_DETAIL_ENDPOINT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_LOCAL_SOCKETS)

#include <cstddef>
#include <string>
#include "netio/detail/socket_types.hpp"
#include "netio/detail/string_view.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace local {
namespace detail {

// Helper class for implementing a UNIX domain endpoint.
class endpoint
{
public:
  // Default constructor.
  NETIO_DECL endpoint() noexcept;

  // Construct an endpoint using the specified path name.
  NETIO_DECL endpoint(const char* path_name);

  // Construct an endpoint using the specified path name.
  NETIO_DECL endpoint(const std::string& path_name);

  #if defined(NETIO_HAS_STRING_VIEW)
  // Construct an endpoint using the specified path name.
  NETIO_DECL endpoint(string_view path_name);
  #endif // defined(NETIO_HAS_STRING_VIEW)

  // Copy constructor.
  endpoint(const endpoint& other) noexcept
    : data_(other.data_),
      path_length_(other.path_length_)
  {
  }

  // Assign from another endpoint.
  endpoint& operator=(const endpoint& other) noexcept
  {
    data_ = other.data_;
    path_length_ = other.path_length_;
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
    return path_length_
      + offsetof(netio::detail::sockaddr_un_type, sun_path);
  }

  // Set the underlying size of the endpoint in the native type.
  NETIO_DECL void resize(std::size_t size);

  // Get the capacity of the endpoint in the native type.
  std::size_t capacity() const noexcept
  {
    return sizeof(netio::detail::sockaddr_un_type);
  }

  // Get the path associated with the endpoint.
  NETIO_DECL std::string path() const;

  // Set the path associated with the endpoint.
  NETIO_DECL void path(const char* p);

  // Set the path associated with the endpoint.
  NETIO_DECL void path(const std::string& p);

  // Compare two endpoints for equality.
  NETIO_DECL friend bool operator==(
      const endpoint& e1, const endpoint& e2) noexcept;

  // Compare endpoints for ordering.
  NETIO_DECL friend bool operator<(
      const endpoint& e1, const endpoint& e2) noexcept;

private:
  // The underlying UNIX socket address.
  union data_union
  {
    netio::detail::socket_addr_type base;
    netio::detail::sockaddr_un_type local;
  } data_;

  // The length of the path associated with the endpoint.
  std::size_t path_length_;

  // Initialise with a specified path.
  NETIO_DECL void init(const char* path, std::size_t path_length);
};

} // namespace detail
} // namespace local
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/local/detail/impl/endpoint.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_LOCAL_SOCKETS)

#endif // NETIO_LOCAL_DETAIL_ENDPOINT_HPP
