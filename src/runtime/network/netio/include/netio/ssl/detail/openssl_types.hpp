#ifndef NETIO_SSL_DETAIL_OPENSSL_TYPES_HPP
#define NETIO_SSL_DETAIL_OPENSSL_TYPES_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/socket_types.hpp"
#if defined(NETIO_USE_WOLFSSL)
# include <wolfssl/options.h>
#endif // defined(NETIO_USE_WOLFSSL)
#include <openssl/conf.h>
#include <openssl/ssl.h>
#if !defined(OPENSSL_NO_ENGINE)
# include <openssl/engine.h>
#endif // !defined(OPENSSL_NO_ENGINE)
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#endif // NETIO_SSL_DETAIL_OPENSSL_TYPES_HPP
