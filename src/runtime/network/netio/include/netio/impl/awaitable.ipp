#ifndef NETIO_IMPL_AWAITABLE_IPP
#define NETIO_IMPL_AWAITABLE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_CO_AWAIT)

#include "netio/awaitable.hpp"
#include "netio/detail/call_stack.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

void awaitable_launch_context::launch(void (*pump_fn)(void*), void* arg)
{
  call_stack<awaitable_launch_context>::context ctx(this);
  pump_fn(arg);
}

bool awaitable_launch_context::is_launching()
{
  return !!call_stack<awaitable_launch_context>::contains(this);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_CO_AWAIT)

#endif // NETIO_IMPL_AWAITABLE_IPP
