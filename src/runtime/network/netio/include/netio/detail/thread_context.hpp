#ifndef NETIO_DETAIL_THREAD_CONTEXT_HPP
#define NETIO_DETAIL_THREAD_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <climits>
#include <cstddef>
#include "netio/detail/call_stack.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class thread_info_base;

// Base class for things that manage threads (scheduler, win_iocp_io_context).
class thread_context
{
public:
  // Obtain a pointer to the top of the thread call stack. Returns null when
  // not running inside a thread context.
  NETIO_DECL static thread_info_base* top_of_thread_call_stack();

protected:
  // Per-thread call stack to track the state of each thread in the context.
  typedef call_stack<thread_context, thread_info_base> thread_call_stack;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/thread_context.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_THREAD_CONTEXT_HPP
