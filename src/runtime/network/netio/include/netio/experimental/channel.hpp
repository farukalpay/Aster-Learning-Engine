#ifndef NETIO_EXPERIMENTAL_CHANNEL_HPP
#define NETIO_EXPERIMENTAL_CHANNEL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/any_io_executor.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/execution/executor.hpp"
#include "netio/is_executor.hpp"
#include "netio/experimental/basic_channel.hpp"
#include "netio/experimental/channel_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace detail {

template <typename ExecutorOrSignature, typename = void>
struct channel_type
{
  template <typename... Signatures>
  struct inner
  {
    typedef basic_channel<any_io_executor, channel_traits<>,
        ExecutorOrSignature, Signatures...> type;
  };
};

template <typename ExecutorOrSignature>
struct channel_type<ExecutorOrSignature,
    enable_if_t<
      is_executor<ExecutorOrSignature>::value
        || execution::is_executor<ExecutorOrSignature>::value
    >>
{
  template <typename... Signatures>
  struct inner
  {
    typedef basic_channel<ExecutorOrSignature,
        channel_traits<>, Signatures...> type;
  };
};

} // namespace detail

/// Template type alias for common use of channel.
#if defined(GENERATING_DOCUMENTATION)
template <typename ExecutorOrSignature, typename... Signatures>
using channel = basic_channel<
    specified_executor_or_any_io_executor, channel_traits<>, signatures...>;
#else // defined(GENERATING_DOCUMENTATION)
template <typename ExecutorOrSignature, typename... Signatures>
using channel = typename detail::channel_type<
    ExecutorOrSignature>::template inner<Signatures...>::type;
#endif // defined(GENERATING_DOCUMENTATION)

} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_CHANNEL_HPP
