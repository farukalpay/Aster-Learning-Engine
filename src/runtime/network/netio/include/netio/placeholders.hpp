#ifndef NETIO_PLACEHOLDERS_HPP
#define NETIO_PLACEHOLDERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/functional.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace placeholders {

#if defined(GENERATING_DOCUMENTATION)

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the error argument of a handler for any of the asynchronous
/// functions.
unspecified error;

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the bytes_transferred argument of a handler for asynchronous
/// functions such as netio::basic_stream_socket::async_write_some or
/// netio::async_write.
unspecified bytes_transferred;

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the iterator argument of a handler for asynchronous functions
/// such as netio::async_connect.
unspecified iterator;

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the results argument of a handler for asynchronous functions
/// such as netio::basic_resolver::async_resolve.
unspecified results;

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the results argument of a handler for asynchronous functions
/// such as netio::async_connect.
unspecified endpoint;

/// An argument placeholder, for use with std::bind() or boost::bind(), that
/// corresponds to the signal_number argument of a handler for asynchronous
/// functions such as netio::signal_set::async_wait.
unspecified signal_number;

#else

static NETIO_INLINE_VARIABLE constexpr auto& error
  = std::placeholders::_1;
static NETIO_INLINE_VARIABLE constexpr auto& bytes_transferred
  = std::placeholders::_2;
static NETIO_INLINE_VARIABLE constexpr auto& iterator
  = std::placeholders::_2;
static NETIO_INLINE_VARIABLE constexpr auto& results
  = std::placeholders::_2;
static NETIO_INLINE_VARIABLE constexpr auto& endpoint
  = std::placeholders::_2;
static NETIO_INLINE_VARIABLE constexpr auto& signal_number
  = std::placeholders::_2;

#endif

} // namespace placeholders
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_PLACEHOLDERS_HPP
