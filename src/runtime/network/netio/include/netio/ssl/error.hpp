#ifndef NETIO_SSL_ERROR_HPP
#define NETIO_SSL_ERROR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/error_code.hpp"
#include "netio/ssl/detail/openssl_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace error {

enum ssl_errors
{
  // Error numbers are those produced by openssl.
};

extern NETIO_DECL
const netio::error_category& get_ssl_category();

static const netio::error_category&
  ssl_category NETIO_UNUSED_VARIABLE
  = netio::error::get_ssl_category();

} // namespace error
namespace ssl {
namespace error {

enum stream_errors
{
#if defined(GENERATING_DOCUMENTATION)
  /// The underlying stream closed before the ssl stream gracefully shut down.
  stream_truncated,

  /// The underlying SSL library returned a system error without providing
  /// further information.
  unspecified_system_error,

  /// The underlying SSL library generated an unexpected result from a function
  /// call.
  unexpected_result
#else // defined(GENERATING_DOCUMENTATION)
# if (OPENSSL_VERSION_NUMBER < 0x10100000L) \
    && !defined(OPENSSL_IS_BORINGSSL) \
    && !defined(NETIO_USE_WOLFSSL)
  stream_truncated = ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ),
# else
  stream_truncated = 1,
# endif
  unspecified_system_error = 2,
  unexpected_result = 3
#endif // defined(GENERATING_DOCUMENTATION)
};

extern NETIO_DECL
const netio::error_category& get_stream_category();

static const netio::error_category&
  stream_category NETIO_UNUSED_VARIABLE
  = netio::ssl::error::get_stream_category();

} // namespace error
} // namespace ssl
} // namespace netio

namespace std {

template<> struct is_error_code_enum<netio::error::ssl_errors>
{
  static const bool value = true;
};

template<> struct is_error_code_enum<netio::ssl::error::stream_errors>
{
  static const bool value = true;
};

} // namespace std

namespace netio {
namespace error {

inline netio::error_code make_error_code(ssl_errors e)
{
  return netio::error_code(
      static_cast<int>(e), get_ssl_category());
}

} // namespace error
namespace ssl {
namespace error {

inline netio::error_code make_error_code(stream_errors e)
{
  return netio::error_code(
      static_cast<int>(e), get_stream_category());
}

} // namespace error
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/ssl/impl/error.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_SSL_ERROR_HPP
