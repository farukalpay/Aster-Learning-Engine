#ifndef NETIO_HANDLER_CONTINUATION_HOOK_HPP
#define NETIO_HANDLER_CONTINUATION_HOOK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// Default continuation function for handlers.
/**
 * Asynchronous operations may represent a continuation of the asynchronous
 * control flow associated with the current handler. The implementation can use
 * this knowledge to optimise scheduling of the handler.
 *
 * Implement netio_handler_is_continuation for your own handlers to indicate
 * when a handler represents a continuation.
 *
 * The default implementation of the continuation hook returns <tt>false</tt>.
 *
 * @par Example
 * @code
 * class my_handler;
 *
 * bool netio_handler_is_continuation(my_handler* context)
 * {
 *   return true;
 * }
 * @endcode
 */
inline bool netio_handler_is_continuation(...)
{
  return false;
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_HANDLER_CONTINUATION_HOOK_HPP
