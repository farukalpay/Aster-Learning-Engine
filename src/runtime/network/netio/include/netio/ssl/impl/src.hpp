#ifndef NETIO_SSL_IMPL_SRC_HPP
#define NETIO_SSL_IMPL_SRC_HPP

#define NETIO_SOURCE

#include "netio/detail/config.hpp"

#if defined(NETIO_HEADER_ONLY)
# error Do not compile Netio library source with NETIO_HEADER_ONLY defined
#endif

#include "netio/ssl/impl/context.ipp"
#include "netio/ssl/impl/error.ipp"
#include "netio/ssl/detail/impl/engine.ipp"
#include "netio/ssl/detail/impl/openssl_init.ipp"
#include "netio/ssl/impl/host_name_verification.ipp"

#endif // NETIO_SSL_IMPL_SRC_HPP
