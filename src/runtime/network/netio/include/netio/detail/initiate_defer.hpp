#ifndef NETIO_DETAIL_INITIATE_DEFER_HPP
#define NETIO_DETAIL_INITIATE_DEFER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/associated_allocator.hpp"
#include "netio/associated_executor.hpp"
#include "netio/detail/work_dispatcher.hpp"
#include "netio/execution/allocator.hpp"
#include "netio/execution/blocking.hpp"
#include "netio/execution/relationship.hpp"
#include "netio/prefer.hpp"
#include "netio/require.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class initiate_defer
{
public:
  template <typename CompletionHandler>
  void operator()(CompletionHandler&& handler,
      enable_if_t<
        execution::is_executor<
          associated_executor_t<decay_t<CompletionHandler>>
        >::value
      >* = 0) const
  {
    associated_executor_t<decay_t<CompletionHandler>> ex(
        (get_associated_executor)(handler));

    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    netio::prefer(
        netio::require(ex, execution::blocking.never),
        execution::relationship.continuation,
        execution::allocator(alloc)
      ).execute(
        netio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)));
  }

  template <typename CompletionHandler>
  void operator()(CompletionHandler&& handler,
      enable_if_t<
        !execution::is_executor<
          associated_executor_t<decay_t<CompletionHandler>>
        >::value
      >* = 0) const
  {
    associated_executor_t<decay_t<CompletionHandler>> ex(
        (get_associated_executor)(handler));

    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    ex.defer(netio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)), alloc);
  }
};

template <typename Executor>
class initiate_defer_with_executor
{
public:
  typedef Executor executor_type;

  explicit initiate_defer_with_executor(const Executor& ex)
    : ex_(ex)
  {
  }

  executor_type get_executor() const noexcept
  {
    return ex_;
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&&,
      enable_if_t<
        execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        !is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    netio::prefer(
        netio::require(ex_, execution::blocking.never),
        execution::relationship.continuation,
        execution::allocator(alloc)
      ).execute(
        netio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)));
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&& function,
      enable_if_t<
        execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    typedef decay_t<CompletionHandler> handler_t;
    typedef decay_t<Function> function_t;

    typedef associated_executor_t<handler_t, Executor> handler_ex_t;
    handler_ex_t handler_ex((get_associated_executor)(handler, ex_));

    associated_allocator_t<handler_t> alloc(
        (get_associated_allocator)(handler));

    netio::prefer(
        netio::require(ex_, execution::blocking.never),
        execution::relationship.continuation,
        execution::allocator(alloc)
      ).execute(
        work_dispatcher<function_t, handler_t, handler_ex_t>(
          static_cast<Function&&>(function),
          static_cast<CompletionHandler&&>(handler), handler_ex));
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&&,
      enable_if_t<
        !execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        !is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    ex_.defer(netio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)), alloc);
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&& function,
      enable_if_t<
        !execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    typedef decay_t<CompletionHandler> handler_t;
    typedef decay_t<Function> function_t;

    typedef associated_executor_t<handler_t, Executor> handler_ex_t;
    handler_ex_t handler_ex((get_associated_executor)(handler, ex_));

    associated_allocator_t<handler_t> alloc(
        (get_associated_allocator)(handler));

    ex_.defer(work_dispatcher<function_t, handler_t, handler_ex_t>(
          static_cast<Function&&>(function),
          static_cast<CompletionHandler&&>(handler), handler_ex), alloc);
  }

private:
  Executor ex_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_INITIATE_DEFER_HPP
