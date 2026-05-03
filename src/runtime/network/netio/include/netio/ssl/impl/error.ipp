#ifndef NETIO_SSL_IMPL_ERROR_IPP
#define NETIO_SSL_IMPL_ERROR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/ssl/error.hpp"
#include "netio/ssl/detail/openssl_init.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace error {
namespace detail {

class ssl_category : public netio::error_category
{
public:
  const char* name() const noexcept
  {
    return "netio.ssl";
  }

  std::string message(int value) const
  {
    const char* reason = ::ERR_reason_error_string(value);
    if (reason)
    {
      const char* lib = ::ERR_lib_error_string(value);
#if (OPENSSL_VERSION_NUMBER < 0x30000000L)
      const char* func = ::ERR_func_error_string(value);
#else // (OPENSSL_VERSION_NUMBER < 0x30000000L)
      const char* func = 0;
#endif // (OPENSSL_VERSION_NUMBER < 0x30000000L)
      std::string result(reason);
      if (lib || func)
      {
        result += " (";
        if (lib)
          result += lib;
        if (lib && func)
          result += ", ";
        if (func)
          result += func;
        result += ")";
      }
      return result;
    }
    return "netio.ssl error";
  }
};

} // namespace detail

const netio::error_category& get_ssl_category()
{
  static detail::ssl_category instance;
  return instance;
}

} // namespace error
namespace ssl {
namespace error {

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) && !defined(OPENSSL_IS_BORINGSSL)

const netio::error_category& get_stream_category()
{
  return netio::error::get_ssl_category();
}

#else

namespace detail {

class stream_category : public netio::error_category
{
public:
  const char* name() const noexcept
  {
    return "netio.ssl.stream";
  }

  std::string message(int value) const
  {
    switch (value)
    {
    case stream_truncated: return "stream truncated";
    case unspecified_system_error: return "unspecified system error";
    case unexpected_result: return "unexpected result";
    default: return "netio.ssl.stream error";
    }
  }
};

} // namespace detail

const netio::error_category& get_stream_category()
{
  static detail::stream_category instance;
  return instance;
}

#endif

} // namespace error
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_IMPL_ERROR_IPP
