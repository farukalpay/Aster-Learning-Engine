#ifndef NETIO_EXPERIMENTAL_IMPL_CHANNEL_ERROR_IPP
#define NETIO_EXPERIMENTAL_IMPL_CHANNEL_ERROR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/experimental/channel_error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace error {
namespace detail {

class channel_category : public netio::error_category
{
public:
  const char* name() const noexcept
  {
    return "netio.channel";
  }

  std::string message(int value) const
  {
    switch (value)
    {
    case channel_closed: return "Channel closed";
    case channel_cancelled: return "Channel cancelled";
    default: return "netio.channel error";
    }
  }
};

} // namespace detail

const netio::error_category& get_channel_category()
{
  static detail::channel_category instance;
  return instance;
}

} // namespace error
} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_IMPL_CHANNEL_ERROR_IPP
