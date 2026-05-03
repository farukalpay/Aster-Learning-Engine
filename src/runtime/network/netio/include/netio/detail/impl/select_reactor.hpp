#ifndef NETIO_DETAIL_IMPL_SELECT_REACTOR_HPP
#define NETIO_DETAIL_IMPL_SELECT_REACTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP) \
  || (!defined(NETIO_HAS_DEV_POLL) \
      && !defined(NETIO_HAS_EPOLL) \
      && !defined(NETIO_HAS_KQUEUE) \
      && !defined(NETIO_WINDOWS_RUNTIME))

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else // defined(NETIO_HAS_IOCP)
# include "netio/detail/scheduler.hpp"
#endif // defined(NETIO_HAS_IOCP)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

inline void select_reactor::post_immediate_completion(
    operation* op, bool is_continuation) const
{
  scheduler_.post_immediate_completion(op, is_continuation);
}

template <typename TimeTraits, typename Allocator>
void select_reactor::add_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_add_timer_queue(queue);
}

// Remove a timer queue from the reactor.
template <typename TimeTraits, typename Allocator>
void select_reactor::remove_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_remove_timer_queue(queue);
}

template <typename TimeTraits, typename Allocator>
void select_reactor::schedule_timer(
    timer_queue<TimeTraits, Allocator>& queue,
    const typename TimeTraits::time_type& time,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
    wait_op* op)
{
  netio::detail::mutex::scoped_lock lock(mutex_);

  if (shutdown_)
  {
    scheduler_.post_immediate_completion(op, false);
    return;
  }

  bool earliest = queue.enqueue_timer(time, timer, op);
  scheduler_.work_started();
  if (earliest)
    interrupter_.interrupt();
}

template <typename TimeTraits, typename Allocator>
std::size_t select_reactor::cancel_timer(
    timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
    std::size_t max_cancelled)
{
  netio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  std::size_t n = queue.cancel_timer(timer, ops, max_cancelled);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
  return n;
}

template <typename TimeTraits, typename Allocator>
void select_reactor::cancel_timer_by_key(
    timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data* timer,
    void* cancellation_key)
{
  mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  queue.cancel_timer_by_key(timer, ops, cancellation_key);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
}

template <typename TimeTraits, typename Allocator>
void select_reactor::move_timer(timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& target,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& source)
{
  netio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  queue.cancel_timer(target, ops);
  queue.move_timer(target, source);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IOCP)
       //   || (!defined(NETIO_HAS_DEV_POLL)
       //       && !defined(NETIO_HAS_EPOLL)
       //       && !defined(NETIO_HAS_KQUEUE)
       //       && !defined(NETIO_WINDOWS_RUNTIME))

#endif // NETIO_DETAIL_IMPL_SELECT_REACTOR_HPP
