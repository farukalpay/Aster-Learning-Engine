#ifndef NETIO_CO_SPAWN_HPP
#define NETIO_CO_SPAWN_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_CO_AWAIT) || defined(GENERATING_DOCUMENTATION)

#include "netio/awaitable.hpp"
#include "netio/execution/executor.hpp"
#include "netio/execution_context.hpp"
#include "netio/is_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
struct awaitable_signature;

template <typename T, typename Executor>
struct awaitable_signature<awaitable<T, Executor>>
{
  typedef void type(std::exception_ptr, T);
};

template <typename Executor>
struct awaitable_signature<awaitable<void, Executor>>
{
  typedef void type(std::exception_ptr);
};

} // namespace detail

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ex The executor that will be used to schedule the new thread of
 * execution.
 *
 * @param a The netio::awaitable object that is the result of calling the
 * coroutine's entry point function.
 *
 * @param token The @ref completion_token that will handle the notification that
 * the thread of execution has completed. The function signature of the
 * completion handler must be:
 * @code void handler(std::exception_ptr, T); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr, T) @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<std::size_t> echo(tcp::socket socket)
 * {
 *   std::size_t bytes_transferred = 0;
 *
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *
 *       bytes_transferred += n;
 *     }
 *   }
 *   catch (const std::exception&)
 *   {
 *   }
 *
 *   co_return bytes_transferred;
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_executor,
 *   echo(std::move(my_tcp_socket)),
 *   [](std::exception_ptr e, std::size_t n)
 *   {
 *     std::cout << "transferred " << n << "\n";
 *   });
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename Executor, typename T, typename AwaitableExecutor,
    NETIO_COMPLETION_TOKEN_FOR(
      void(std::exception_ptr, T)) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
inline NETIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken, void(std::exception_ptr, T))
co_spawn(const Executor& ex, awaitable<T, AwaitableExecutor> a,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(Executor),
    constraint_t<
      (is_executor<Executor>::value || execution::is_executor<Executor>::value)
        && is_convertible<Executor, AwaitableExecutor>::value
    > = 0);

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ex The executor that will be used to schedule the new thread of
 * execution.
 *
 * @param a The netio::awaitable object that is the result of calling the
 * coroutine's entry point function.
 *
 * @param token The @ref completion_token that will handle the notification that
 * the thread of execution has completed. The function signature of the
 * completion handler must be:
 * @code void handler(std::exception_ptr); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr) @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<void> echo(tcp::socket socket)
 * {
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *     }
 *   }
 *   catch (const std::exception& e)
 *   {
 *     std::cerr << "Exception: " << e.what() << "\n";
 *   }
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_executor,
 *   echo(std::move(my_tcp_socket)),
 *   netio::detached);
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename Executor, typename AwaitableExecutor,
    NETIO_COMPLETION_TOKEN_FOR(
      void(std::exception_ptr)) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
inline NETIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken, void(std::exception_ptr))
co_spawn(const Executor& ex, awaitable<void, AwaitableExecutor> a,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(Executor),
    constraint_t<
      (is_executor<Executor>::value || execution::is_executor<Executor>::value)
        && is_convertible<Executor, AwaitableExecutor>::value
    > = 0);

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ctx An execution context that will provide the executor to be used to
 * schedule the new thread of execution.
 *
 * @param a The netio::awaitable object that is the result of calling the
 * coroutine's entry point function.
 *
 * @param token The @ref completion_token that will handle the notification that
 * the thread of execution has completed. The function signature of the
 * completion handler must be:
 * @code void handler(std::exception_ptr); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr, T) @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<std::size_t> echo(tcp::socket socket)
 * {
 *   std::size_t bytes_transferred = 0;
 *
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *
 *       bytes_transferred += n;
 *     }
 *   }
 *   catch (const std::exception&)
 *   {
 *   }
 *
 *   co_return bytes_transferred;
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_io_context,
 *   echo(std::move(my_tcp_socket)),
 *   [](std::exception_ptr e, std::size_t n)
 *   {
 *     std::cout << "transferred " << n << "\n";
 *   });
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename ExecutionContext, typename T, typename AwaitableExecutor,
    NETIO_COMPLETION_TOKEN_FOR(
      void(std::exception_ptr, T)) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(
          typename ExecutionContext::executor_type)>
inline NETIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken, void(std::exception_ptr, T))
co_spawn(ExecutionContext& ctx, awaitable<T, AwaitableExecutor> a,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(
        typename ExecutionContext::executor_type),
    constraint_t<
      is_convertible<ExecutionContext&, execution_context&>::value
        && is_convertible<typename ExecutionContext::executor_type,
          AwaitableExecutor>::value
    > = 0);

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ctx An execution context that will provide the executor to be used to
 * schedule the new thread of execution.
 *
 * @param a The netio::awaitable object that is the result of calling the
 * coroutine's entry point function.
 *
 * @param token The @ref completion_token that will handle the notification that
 * the thread of execution has completed. The function signature of the
 * completion handler must be:
 * @code void handler(std::exception_ptr); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr) @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<void> echo(tcp::socket socket)
 * {
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *     }
 *   }
 *   catch (const std::exception& e)
 *   {
 *     std::cerr << "Exception: " << e.what() << "\n";
 *   }
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_io_context,
 *   echo(std::move(my_tcp_socket)),
 *   netio::detached);
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename ExecutionContext, typename AwaitableExecutor,
    NETIO_COMPLETION_TOKEN_FOR(
      void(std::exception_ptr)) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(
          typename ExecutionContext::executor_type)>
inline NETIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken, void(std::exception_ptr))
co_spawn(ExecutionContext& ctx, awaitable<void, AwaitableExecutor> a,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(
        typename ExecutionContext::executor_type),
    constraint_t<
      is_convertible<ExecutionContext&, execution_context&>::value
        && is_convertible<typename ExecutionContext::executor_type,
          AwaitableExecutor>::value
    > = 0);

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ex The executor that will be used to schedule the new thread of
 * execution.
 *
 * @param f A nullary function object with a return type of the form
 * @c netio::awaitable<R,E> that will be used as the coroutine's entry
 * point.
 *
 * @param token The @ref completion_token that will handle the notification
 * that the thread of execution has completed. If @c R is @c void, the function
 * signature of the completion handler must be:
 *
 * @code void handler(std::exception_ptr); @endcode
 * Otherwise, the function signature of the completion handler must be:
 * @code void handler(std::exception_ptr, R); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr, R) @endcode
 * where @c R is the first template argument to the @c awaitable returned by the
 * supplied function object @c F:
 * @code netio::awaitable<R, AwaitableExecutor> F() @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<std::size_t> echo(tcp::socket socket)
 * {
 *   std::size_t bytes_transferred = 0;
 *
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *
 *       bytes_transferred += n;
 *     }
 *   }
 *   catch (const std::exception&)
 *   {
 *   }
 *
 *   co_return bytes_transferred;
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_executor,
 *   [socket = std::move(my_tcp_socket)]() mutable
 *     -> netio::awaitable<void>
 *   {
 *     try
 *     {
 *       char data[1024];
 *       for (;;)
 *       {
 *         std::size_t n = co_await socket.async_read_some(
 *             netio::buffer(data), netio::use_awaitable);
 *
 *         co_await netio::async_write(socket,
 *             netio::buffer(data, n), netio::use_awaitable);
 *       }
 *     }
 *     catch (const std::exception& e)
 *     {
 *       std::cerr << "Exception: " << e.what() << "\n";
 *     }
 *   }, netio::detached);
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename Executor, typename F,
    NETIO_COMPLETION_TOKEN_FOR(typename detail::awaitable_signature<
      result_of_t<F()>>::type) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
NETIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    typename detail::awaitable_signature<result_of_t<F()>>::type)
co_spawn(const Executor& ex, F&& f,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(Executor),
    constraint_t<
      is_executor<Executor>::value || execution::is_executor<Executor>::value
    > = 0);

/// Spawn a new coroutined-based thread of execution.
/**
 * @param ctx An execution context that will provide the executor to be used to
 * schedule the new thread of execution.
 *
 * @param f A nullary function object with a return type of the form
 * @c netio::awaitable<R,E> that will be used as the coroutine's entry
 * point.
 *
 * @param token The @ref completion_token that will handle the notification
 * that the thread of execution has completed. If @c R is @c void, the function
 * signature of the completion handler must be:
 *
 * @code void handler(std::exception_ptr); @endcode
 * Otherwise, the function signature of the completion handler must be:
 * @code void handler(std::exception_ptr, R); @endcode
 *
 * @par Completion Signature
 * @code void(std::exception_ptr, R) @endcode
 * where @c R is the first template argument to the @c awaitable returned by the
 * supplied function object @c F:
 * @code netio::awaitable<R, AwaitableExecutor> F() @endcode
 *
 * @par Example
 * @code
 * netio::awaitable<std::size_t> echo(tcp::socket socket)
 * {
 *   std::size_t bytes_transferred = 0;
 *
 *   try
 *   {
 *     char data[1024];
 *     for (;;)
 *     {
 *       std::size_t n = co_await socket.async_read_some(
 *           netio::buffer(data), netio::use_awaitable);
 *
 *       co_await netio::async_write(socket,
 *           netio::buffer(data, n), netio::use_awaitable);
 *
 *       bytes_transferred += n;
 *     }
 *   }
 *   catch (const std::exception&)
 *   {
 *   }
 *
 *   co_return bytes_transferred;
 * }
 *
 * // ...
 *
 * netio::co_spawn(my_io_context,
 *   [socket = std::move(my_tcp_socket)]() mutable
 *     -> netio::awaitable<void>
 *   {
 *     try
 *     {
 *       char data[1024];
 *       for (;;)
 *       {
 *         std::size_t n = co_await socket.async_read_some(
 *             netio::buffer(data), netio::use_awaitable);
 *
 *         co_await netio::async_write(socket,
 *             netio::buffer(data, n), netio::use_awaitable);
 *       }
 *     }
 *     catch (const std::exception& e)
 *     {
 *       std::cerr << "Exception: " << e.what() << "\n";
 *     }
 *   }, netio::detached);
 * @endcode
 *
 * @par Per-Operation Cancellation
 * The new thread of execution is created with a cancellation state that
 * supports @c cancellation_type::terminal values only. To change the
 * cancellation state, call netio::this_coro::reset_cancellation_state.
 */
template <typename ExecutionContext, typename F,
    NETIO_COMPLETION_TOKEN_FOR(typename detail::awaitable_signature<
      result_of_t<F()>>::type) CompletionToken
        NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(
          typename ExecutionContext::executor_type)>
NETIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
    typename detail::awaitable_signature<result_of_t<F()>>::type)
co_spawn(ExecutionContext& ctx, F&& f,
    CompletionToken&& token
      NETIO_DEFAULT_COMPLETION_TOKEN(
        typename ExecutionContext::executor_type),
    constraint_t<
      is_convertible<ExecutionContext&, execution_context&>::value
    > = 0);

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/co_spawn.hpp"

#endif // defined(NETIO_HAS_CO_AWAIT) || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_CO_SPAWN_HPP
