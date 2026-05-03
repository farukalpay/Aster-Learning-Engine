#ifndef NETIO_DETAIL_IMPL_THREAD_CONTEXT_IPP
#define NETIO_DETAIL_IMPL_THREAD_CONTEXT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

thread_info_base* thread_context::top_of_thread_call_stack()
{
  return thread_call_stack::top();
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_IMPL_THREAD_CONTEXT_IPP
