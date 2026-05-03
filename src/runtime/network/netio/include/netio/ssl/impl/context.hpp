#ifndef NETIO_SSL_IMPL_CONTEXT_HPP
#define NETIO_SSL_IMPL_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/throw_error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {

template <typename VerifyCallback>
void context::set_verify_callback(VerifyCallback callback)
{
  netio::error_code ec;
  this->set_verify_callback(callback, ec);
  netio::detail::throw_error(ec, "set_verify_callback");
}

template <typename VerifyCallback>
NETIO_SYNC_OP_VOID context::set_verify_callback(
    VerifyCallback callback, netio::error_code& ec)
{
  do_set_verify_callback(
      new detail::verify_callback<VerifyCallback>(callback), ec);
  NETIO_SYNC_OP_VOID_RETURN(ec);
}

template <typename PasswordCallback>
void context::set_password_callback(PasswordCallback callback)
{
  netio::error_code ec;
  this->set_password_callback(callback, ec);
  netio::detail::throw_error(ec, "set_password_callback");
}

template <typename PasswordCallback>
NETIO_SYNC_OP_VOID context::set_password_callback(
    PasswordCallback callback, netio::error_code& ec)
{
  do_set_password_callback(
      new detail::password_callback<PasswordCallback>(callback), ec);
  NETIO_SYNC_OP_VOID_RETURN(ec);
}

} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_IMPL_CONTEXT_HPP
