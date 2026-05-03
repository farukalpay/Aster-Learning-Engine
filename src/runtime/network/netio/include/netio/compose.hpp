#ifndef NETIO_COMPOSE_HPP
#define NETIO_COMPOSE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/composed.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// Launch an asynchronous operation with a stateful implementation.
/**
 * The async_compose function simplifies the implementation of composed
 * asynchronous operations automatically wrapping a stateful function object
 * with a conforming intermediate completion handler.
 *
 * @param implementation A function object that contains the implementation of
 * the composed asynchronous operation. The first argument to the function
 * object is a non-const reference to the enclosing intermediate completion
 * handler. The remaining arguments are any arguments that originate from the
 * completion handlers of any asynchronous operations performed by the
 * implementation.
 *
 * @param token The completion token.
 *
 * @param io_objects_or_executors Zero or more I/O objects or I/O executors for
 * which outstanding work must be maintained.
 *
 * @par Per-Operation Cancellation
 * By default, terminal per-operation cancellation is enabled for
 * composed operations that are implemented using @c async_compose. To
 * disable cancellation for the composed operation, or to alter its
 * supported cancellation types, call the @c self object's @c
 * reset_cancellation_state function.
 *
 * @par Example:
 *
 * @code struct async_echo_implementation
 * {
 *   tcp::socket& socket_;
 *   netio::mutable_buffer buffer_;
 *   enum { starting, reading, writing } state_;
 *
 *   template <typename Self>
 *   void operator()(Self& self,
 *       netio::error_code error = {},
 *       std::size_t n = 0)
 *   {
 *     switch (state_)
 *     {
 *     case starting:
 *       state_ = reading;
 *       socket_.async_read_some(
 *           buffer_, std::move(self));
 *       break;
 *     case reading:
 *       if (error)
 *       {
 *         self.complete(error, 0);
 *       }
 *       else
 *       {
 *         state_ = writing;
 *         netio::async_write(socket_, buffer_,
 *             netio::transfer_exactly(n),
 *             std::move(self));
 *       }
 *       break;
 *     case writing:
 *       self.complete(error, n);
 *       break;
 *     }
 *   }
 * };
 *
 * template <typename CompletionToken>
 * auto async_echo(tcp::socket& socket,
 *     netio::mutable_buffer buffer,
 *     CompletionToken&& token)
 *   -> decltype(
 *     netio::async_compose<CompletionToken,
 *       void(netio::error_code, std::size_t)>(
 *         std::declval<async_echo_implementation>(),
 *         token, socket))
 * {
 *   return netio::async_compose<CompletionToken,
 *     void(netio::error_code, std::size_t)>(
 *       async_echo_implementation{socket, buffer,
 *         async_echo_implementation::starting},
 *       token, socket);
 * } @endcode
 */
template <typename CompletionToken, typename Signature,
    typename Implementation, typename... IoObjectsOrExecutors>
inline auto async_compose(Implementation&& implementation,
    type_identity_t<CompletionToken>& token,
    IoObjectsOrExecutors&&... io_objects_or_executors)
  -> decltype(
    async_initiate<CompletionToken, Signature>(
      composed<Signature>(static_cast<Implementation&&>(implementation),
        static_cast<IoObjectsOrExecutors&&>(io_objects_or_executors)...),
      token))
{
  return async_initiate<CompletionToken, Signature>(
      composed<Signature>(static_cast<Implementation&&>(implementation),
        static_cast<IoObjectsOrExecutors&&>(io_objects_or_executors)...),
      token);
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_COMPOSE_HPP
