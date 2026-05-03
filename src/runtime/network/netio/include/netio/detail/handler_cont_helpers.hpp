#ifndef NETIO_DETAIL_HANDLER_CONT_HELPERS_HPP
#define NETIO_DETAIL_HANDLER_CONT_HELPERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/handler_continuation_hook.hpp"

#include "netio/detail/push_options.hpp"

// Calls to netio_handler_is_continuation must be made from a namespace that
// does not contain overloads of this function. This namespace is defined here
// for that purpose.
namespace netio_handler_cont_helpers {

template <typename Context>
inline bool is_continuation(Context& context)
{
#if !defined(NETIO_HAS_HANDLER_HOOKS)
  return false;
#else
  using netio::netio_handler_is_continuation;
  return netio_handler_is_continuation(
      netio::detail::addressof(context));
#endif
}

} // namespace netio_handler_cont_helpers

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_HANDLER_CONT_HELPERS_HPP
