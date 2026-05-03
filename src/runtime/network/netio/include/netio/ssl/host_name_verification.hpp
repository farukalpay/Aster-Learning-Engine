#ifndef NETIO_SSL_HOST_NAME_VERIFICATION_HPP
#define NETIO_SSL_HOST_NAME_VERIFICATION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include <string>
#include "netio/ssl/detail/openssl_types.hpp"
#include "netio/ssl/verify_context.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {

/// Verifies a certificate against a host_name according to the rules described
/// in RFC 6125.
/**
 * @par Example
 * The following example shows how to synchronously open a secure connection to
 * a given host name:
 * @code
 * using netio::ip::tcp;
 * namespace ssl = netio::ssl;
 * typedef ssl::stream<tcp::socket> ssl_socket;
 *
 * // Create a context that uses the default paths for finding CA certificates.
 * ssl::context ctx(ssl::context::sslv23);
 * ctx.set_default_verify_paths();
 *
 * // Open a socket and connect it to the remote host.
 * netio::io_context io_context;
 * ssl_socket sock(io_context, ctx);
 * tcp::resolver resolver(io_context);
 * tcp::resolver::query query("host.name", "https");
 * netio::connect(sock.lowest_layer(), resolver.resolve(query));
 * sock.lowest_layer().set_option(tcp::no_delay(true));
 *
 * // Perform SSL handshake and verify the remote host's certificate.
 * sock.set_verify_mode(ssl::verify_peer);
 * sock.set_verify_callback(ssl::host_name_verification("host.name"));
 * sock.handshake(ssl_socket::client);
 *
 * // ... read and write as normal ...
 * @endcode
 */
class host_name_verification
{
public:
  /// The type of the function object's result.
  typedef bool result_type;

  /// Constructor.
  explicit host_name_verification(const std::string& host)
    : host_(host)
  {
  }

  /// Perform certificate verification.
  NETIO_DECL bool operator()(bool preverified, verify_context& ctx) const;

private:
  // Helper function to check a host name against an IPv4 address
  // The host name to be checked.
  std::string host_;
};

} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/ssl/impl/host_name_verification.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_SSL_HOST_NAME_VERIFICATION_HPP
