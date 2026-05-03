#ifndef NETIO_EXPERIMENTAL_CHANNEL_ERROR_HPP
#define NETIO_EXPERIMENTAL_CHANNEL_ERROR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/error_code.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace error {

enum channel_errors
{
  /// The channel was closed.
  channel_closed = 1,

  /// The channel was cancelled.
  channel_cancelled = 2
};

extern NETIO_DECL
const netio::error_category& get_channel_category();

static const netio::error_category&
  channel_category NETIO_UNUSED_VARIABLE
  = netio::experimental::error::get_channel_category();

} // namespace error
namespace channel_errc {
  // Simulates a scoped enum.
  using error::channel_closed;
  using error::channel_cancelled;
} // namespace channel_errc
} // namespace experimental
} // namespace netio

namespace std {

template<> struct is_error_code_enum<
    netio::experimental::error::channel_errors>
{
  static const bool value = true;
};

} // namespace std

namespace netio {
namespace experimental {
namespace error {

inline netio::error_code make_error_code(channel_errors e)
{
  return netio::error_code(
      static_cast<int>(e), get_channel_category());
}

} // namespace error
} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/experimental/impl/channel_error.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_EXPERIMENTAL_CHANNEL_ERROR_HPP
