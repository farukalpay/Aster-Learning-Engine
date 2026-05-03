#ifndef NETIO_IP_ADDRESS_HPP
#define NETIO_IP_ADDRESS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <functional>
#include <string>
#include "netio/detail/throw_exception.hpp"
#include "netio/detail/string_view.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/error_code.hpp"
#include "netio/ip/address_v4.hpp"
#include "netio/ip/address_v6.hpp"
#include "netio/ip/bad_address_cast.hpp"

#if !defined(NETIO_NO_IOSTREAM)
# include <iosfwd>
#endif // !defined(NETIO_NO_IOSTREAM)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// Implements version-independent IP addresses.
/**
 * The netio::ip::address class provides the ability to use either IP
 * version 4 or version 6 addresses.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Unsafe.
 */
class address
{
public:
  /// Default constructor.
  NETIO_DECL address() noexcept;

  /// Construct an address from an IPv4 address.
  NETIO_DECL address(
      const netio::ip::address_v4& ipv4_address) noexcept;

  /// Construct an address from an IPv6 address.
  NETIO_DECL address(
      const netio::ip::address_v6& ipv6_address) noexcept;

  /// Copy constructor.
  NETIO_DECL address(const address& other) noexcept;

  /// Move constructor.
  NETIO_DECL address(address&& other) noexcept;

  /// Assign from another address.
  NETIO_DECL address& operator=(const address& other) noexcept;

  /// Move-assign from another address.
  NETIO_DECL address& operator=(address&& other) noexcept;

  /// Assign from an IPv4 address.
  NETIO_DECL address& operator=(
      const netio::ip::address_v4& ipv4_address) noexcept;

  /// Assign from an IPv6 address.
  NETIO_DECL address& operator=(
      const netio::ip::address_v6& ipv6_address) noexcept;

  /// Get whether the address is an IP version 4 address.
  bool is_v4() const noexcept
  {
    return type_ == ipv4;
  }

  /// Get whether the address is an IP version 6 address.
  bool is_v6() const noexcept
  {
    return type_ == ipv6;
  }

  /// Get the address as an IP version 4 address.
  NETIO_DECL netio::ip::address_v4 to_v4() const;

  /// Get the address as an IP version 6 address.
  NETIO_DECL netio::ip::address_v6 to_v6() const;

  /// Get the address as a string.
  NETIO_DECL std::string to_string() const;

  /// Determine whether the address is a loopback address.
  NETIO_DECL bool is_loopback() const noexcept;

  /// Determine whether the address is unspecified.
  NETIO_DECL bool is_unspecified() const noexcept;

  /// Determine whether the address is a multicast address.
  NETIO_DECL bool is_multicast() const noexcept;

  /// Compare two addresses for equality.
  NETIO_DECL friend bool operator==(const address& a1,
      const address& a2) noexcept;

  /// Compare two addresses for inequality.
  friend bool operator!=(const address& a1,
      const address& a2) noexcept
  {
    return !(a1 == a2);
  }

  /// Compare addresses for ordering.
  NETIO_DECL friend bool operator<(const address& a1,
      const address& a2) noexcept;

  /// Compare addresses for ordering.
  friend bool operator>(const address& a1,
      const address& a2) noexcept
  {
    return a2 < a1;
  }

  /// Compare addresses for ordering.
  friend bool operator<=(const address& a1,
      const address& a2) noexcept
  {
    return !(a2 < a1);
  }

  /// Compare addresses for ordering.
  friend bool operator>=(const address& a1,
      const address& a2) noexcept
  {
    return !(a1 < a2);
  }

private:
  // The type of the address.
  enum { ipv4, ipv6 } type_;

  // The underlying IPv4 address.
  netio::ip::address_v4 ipv4_address_;

  // The underlying IPv6 address.
  netio::ip::address_v6 ipv6_address_;
};

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(const char* str);

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(const char* str,
    netio::error_code& ec) noexcept;

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(const std::string& str);

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(const std::string& str,
    netio::error_code& ec) noexcept;

#if defined(NETIO_HAS_STRING_VIEW) \
  || defined(GENERATING_DOCUMENTATION)

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(string_view str);

/// Create an address from an IPv4 address string in dotted decimal form,
/// or from an IPv6 address in hexadecimal notation.
/**
 * @relates address
 */
NETIO_DECL address make_address(string_view str,
    netio::error_code& ec) noexcept;

#endif // defined(NETIO_HAS_STRING_VIEW)
       //  || defined(GENERATING_DOCUMENTATION)

#if !defined(NETIO_NO_IOSTREAM)

/// Output an address as a string.
/**
 * Used to output a human-readable string for a specified address.
 *
 * @param os The output stream to which the string will be written.
 *
 * @param addr The address to be written.
 *
 * @return The output stream.
 *
 * @relates netio::ip::address
 */
template <typename Elem, typename Traits>
std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const address& addr);

#endif // !defined(NETIO_NO_IOSTREAM)

} // namespace ip
} // namespace netio

namespace std {

template <>
struct hash<netio::ip::address>
{
  std::size_t operator()(const netio::ip::address& addr)
    const noexcept
  {
    return addr.is_v4()
      ? std::hash<netio::ip::address_v4>()(addr.to_v4())
      : std::hash<netio::ip::address_v6>()(addr.to_v6());
  }
};

} // namespace std

#include "netio/detail/pop_options.hpp"

#include "netio/ip/impl/address.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/ip/impl/address.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_IP_ADDRESS_HPP
