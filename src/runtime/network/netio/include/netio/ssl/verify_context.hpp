#ifndef NETIO_SSL_VERIFY_CONTEXT_HPP
#define NETIO_SSL_VERIFY_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/noncopyable.hpp"
#include "netio/ssl/detail/openssl_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {

/// A simple wrapper around the X509_STORE_CTX type, used during verification of
/// a peer certificate.
/**
 * @note The verify_context does not own the underlying X509_STORE_CTX object.
 */
class verify_context
  : private noncopyable
{
public:
  /// The native handle type of the verification context.
  typedef X509_STORE_CTX* native_handle_type;

  /// Constructor.
  explicit verify_context(native_handle_type handle)
    : handle_(handle)
  {
  }

  /// Get the underlying implementation in the native type.
  /**
   * This function may be used to obtain the underlying implementation of the
   * context. This is intended to allow access to context functionality that is
   * not otherwise provided.
   */
  native_handle_type native_handle()
  {
    return handle_;
  }

private:
  // The underlying native implementation.
  native_handle_type handle_;
};

} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_VERIFY_CONTEXT_HPP
