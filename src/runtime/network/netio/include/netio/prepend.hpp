#ifndef NETIO_PREPEND_HPP
#define NETIO_PREPEND_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <tuple>
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// Completion token type used to specify that the completion handler
/// arguments should be passed additional values before the results of the
/// operation.
template <typename CompletionToken, typename... Values>
class prepend_t
{
public:
  /// Constructor.
  template <typename T, typename... V>
  constexpr explicit prepend_t(T&& completion_token, V&&... values)
    : token_(static_cast<T&&>(completion_token)),
      values_(static_cast<V&&>(values)...)
  {
  }

//private:
  CompletionToken token_;
  std::tuple<Values...> values_;
};

/// Completion token type used to specify that the completion handler
/// arguments should be passed additional values before the results of the
/// operation.
template <typename CompletionToken, typename... Values>
NETIO_NODISCARD inline constexpr
prepend_t<decay_t<CompletionToken>, decay_t<Values>...>
prepend(CompletionToken&& completion_token,
    Values&&... values)
{
  return prepend_t<decay_t<CompletionToken>, decay_t<Values>...>(
      static_cast<CompletionToken&&>(completion_token),
      static_cast<Values&&>(values)...);
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/prepend.hpp"

#endif // NETIO_PREPEND_HPP
