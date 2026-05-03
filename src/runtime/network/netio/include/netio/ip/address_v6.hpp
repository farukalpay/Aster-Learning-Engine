#ifndef NETIO_IP_ADDRESS_V6_HPP
#define NETIO_IP_ADDRESS_V6_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <functional>
#include <string>
#include "netio/detail/array.hpp"
#include "netio/detail/cstdint.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/detail/string_view.hpp"
#include "netio/detail/winsock_init.hpp"
#include "netio/error_code.hpp"
#include "netio/ip/address_v4.hpp"

#if !defined(NETIO_NO_IOSTREAM)
# include <iosfwd>
#endif // !defined(NETIO_NO_IOSTREAM)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

template <typename> class basic_address_iterator;

/// Type used for storing IPv6 scope IDs.
typedef uint_least32_t scope_id_type;

/// Implements IP version 6 style addresses.
/**
 * The netio::ip::address_v6 class provides the ability to use and
 * manipulate IP version 6 addresses.
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Unsafe.
 */
class address_v6
{
public:
  /// The type used to represent an address as an array of bytes.
  /**
   * @note This type is defined in terms of the C++0x template @c std::array
   * when it is available. Otherwise, it uses @c boost:array.
   */
#if defined(GENERATING_DOCUMENTATION)
  typedef array<unsigned char, 16> bytes_type;
#else
  typedef netio::detail::array<unsigned char, 16> bytes_type;
#endif

  /// Default constructor.
  /**
   * Initialises the @c address_v6 object such that:
   * @li <tt>to_bytes()</tt> yields <tt>{0, 0, ..., 0}</tt>; and
   * @li <tt>scope_id() == 0</tt>.
   */
  NETIO_DECL address_v6() noexcept;

  /// Construct an address from raw bytes and scope ID.
  /**
   * Initialises the @c address_v6 object such that:
   * @li <tt>to_bytes() == bytes</tt>; and
   * @li <tt>this->scope_id() == scope_id</tt>.
   *
   * @throws out_of_range Thrown if any element in @c bytes is not in the range
   * <tt>0 - 0xFF</tt>. Note that no range checking is required for platforms
   * where <tt>std::numeric_limits<unsigned char>::max()</tt> is <tt>0xFF</tt>.
   */
  NETIO_DECL explicit address_v6(const bytes_type& bytes,
      scope_id_type scope_id = 0);

  /// Copy constructor.
  NETIO_DECL address_v6(const address_v6& other) noexcept;

  /// Move constructor.
  NETIO_DECL address_v6(address_v6&& other) noexcept;

  /// Assign from another address.
  NETIO_DECL address_v6& operator=(
      const address_v6& other) noexcept;

  /// Move-assign from another address.
  NETIO_DECL address_v6& operator=(address_v6&& other) noexcept;

  /// The scope ID of the address.
  /**
   * Returns the scope ID associated with the IPv6 address.
   */
  scope_id_type scope_id() const noexcept
  {
    return scope_id_;
  }

  /// The scope ID of the address.
  /**
   * Modifies the scope ID associated with the IPv6 address.
   *
   * @param id The new scope ID.
   */
  void scope_id(scope_id_type id) noexcept
  {
    scope_id_ = id;
  }

  /// Get the address in bytes, in network byte order.
  NETIO_DECL bytes_type to_bytes() const noexcept;

  /// Get the address as a string.
  NETIO_DECL std::string to_string() const;

  /// Determine whether the address is a loopback address.
  /**
   * This function tests whether the address is the loopback address
   * <tt>::1</tt>.
   */
  NETIO_DECL bool is_loopback() const noexcept;

  /// Determine whether the address is unspecified.
  /**
   * This function tests whether the address is the loopback address
   * <tt>::</tt>.
   */
  NETIO_DECL bool is_unspecified() const noexcept;

  /// Determine whether the address is link local.
  NETIO_DECL bool is_link_local() const noexcept;

  /// Determine whether the address is site local.
  NETIO_DECL bool is_site_local() const noexcept;

  /// Determine whether the address is a mapped IPv4 address.
  NETIO_DECL bool is_v4_mapped() const noexcept;

  /// Determine whether the address is a multicast address.
  NETIO_DECL bool is_multicast() const noexcept;

  /// Determine whether the address is a global multicast address.
  NETIO_DECL bool is_multicast_global() const noexcept;

  /// Determine whether the address is a link-local multicast address.
  NETIO_DECL bool is_multicast_link_local() const noexcept;

  /// Determine whether the address is a node-local multicast address.
  NETIO_DECL bool is_multicast_node_local() const noexcept;

  /// Determine whether the address is a org-local multicast address.
  NETIO_DECL bool is_multicast_org_local() const noexcept;

  /// Determine whether the address is a site-local multicast address.
  NETIO_DECL bool is_multicast_site_local() const noexcept;

  /// Compare two addresses for equality.
  NETIO_DECL friend bool operator==(const address_v6& a1,
      const address_v6& a2) noexcept;

  /// Compare two addresses for inequality.
  friend bool operator!=(const address_v6& a1,
      const address_v6& a2) noexcept
  {
    return !(a1 == a2);
  }

  /// Compare addresses for ordering.
  NETIO_DECL friend bool operator<(const address_v6& a1,
      const address_v6& a2) noexcept;

  /// Compare addresses for ordering.
  friend bool operator>(const address_v6& a1,
      const address_v6& a2) noexcept
  {
    return a2 < a1;
  }

  /// Compare addresses for ordering.
  friend bool operator<=(const address_v6& a1,
      const address_v6& a2) noexcept
  {
    return !(a2 < a1);
  }

  /// Compare addresses for ordering.
  friend bool operator>=(const address_v6& a1,
      const address_v6& a2) noexcept
  {
    return !(a1 < a2);
  }

  /// Obtain an address object that represents any address.
  /**
   * This functions returns an address that represents the "any" address
   * <tt>::</tt>.
   *
   * @returns A default-constructed @c address_v6 object.
   */
  static address_v6 any() noexcept
  {
    return address_v6();
  }

  /// Obtain an address object that represents the loopback address.
  /**
   * This function returns an address that represents the well-known loopback
   * address <tt>::1</tt>.
   */
  NETIO_DECL static address_v6 loopback() noexcept;

private:
  friend class basic_address_iterator<address_v6>;

  // The underlying IPv6 address.
  netio::detail::in6_addr_type addr_;

  // The scope ID associated with the address.
  scope_id_type scope_id_;
};

/// Create an IPv6 address from raw bytes and scope ID.
/**
 * @relates address_v6
 */
inline address_v6 make_address_v6(const address_v6::bytes_type& bytes,
    scope_id_type scope_id = 0)
{
  return address_v6(bytes, scope_id);
}

/// Create an IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(const char* str);

/// Create an IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(const char* str,
    netio::error_code& ec) noexcept;

/// Createan IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(const std::string& str);

/// Create an IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(const std::string& str,
    netio::error_code& ec) noexcept;

#if defined(NETIO_HAS_STRING_VIEW) \
  || defined(GENERATING_DOCUMENTATION)

/// Create an IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(string_view str);

/// Create an IPv6 address from an IP address string.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(string_view str,
    netio::error_code& ec) noexcept;

#endif // defined(NETIO_HAS_STRING_VIEW)
       //  || defined(GENERATING_DOCUMENTATION)

/// Tag type used for distinguishing overloads that deal in IPv4-mapped IPv6
/// addresses.
enum v4_mapped_t { v4_mapped };

/// Create an IPv4 address from a IPv4-mapped IPv6 address.
/**
 * @relates address_v4
 */
NETIO_DECL address_v4 make_address_v4(
    v4_mapped_t, const address_v6& v6_addr);

/// Create an IPv4-mapped IPv6 address from an IPv4 address.
/**
 * @relates address_v6
 */
NETIO_DECL address_v6 make_address_v6(
    v4_mapped_t, const address_v4& v4_addr);

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
 * @relates netio::ip::address_v6
 */
template <typename Elem, typename Traits>
std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const address_v6& addr);

#endif // !defined(NETIO_NO_IOSTREAM)

} // namespace ip
} // namespace netio

namespace std {

template <>
struct hash<netio::ip::address_v6>
{
  std::size_t operator()(const netio::ip::address_v6& addr)
    const noexcept
  {
    const netio::ip::address_v6::bytes_type bytes = addr.to_bytes();
    std::size_t result = static_cast<std::size_t>(addr.scope_id());
    combine_4_bytes(result, &bytes[0]);
    combine_4_bytes(result, &bytes[4]);
    combine_4_bytes(result, &bytes[8]);
    combine_4_bytes(result, &bytes[12]);
    return result;
  }

private:
  static void combine_4_bytes(std::size_t& seed, const unsigned char* bytes)
  {
    const std::size_t bytes_hash =
      (static_cast<std::size_t>(bytes[0]) << 24) |
      (static_cast<std::size_t>(bytes[1]) << 16) |
      (static_cast<std::size_t>(bytes[2]) << 8) |
      (static_cast<std::size_t>(bytes[3]));
    seed ^= bytes_hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

} // namespace std

#include "netio/detail/pop_options.hpp"

#include "netio/ip/impl/address_v6.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/ip/impl/address_v6.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_IP_ADDRESS_V6_HPP
