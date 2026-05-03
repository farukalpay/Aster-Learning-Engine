#ifndef NETIO_IMPL_SYSTEM_CONTEXT_HPP
#define NETIO_IMPL_SYSTEM_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/system_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

inline system_context::executor_type
system_context::get_executor() noexcept
{
  return system_executor();
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_SYSTEM_CONTEXT_HPP
