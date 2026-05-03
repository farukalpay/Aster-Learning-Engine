#ifndef NETIO_DETAIL_IMPL_WINRT_TIMER_SCHEDULER_HPP
#define NETIO_DETAIL_IMPL_WINRT_TIMER_SCHEDULER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename TimeTraits, typename Allocator>
void winrt_timer_scheduler::add_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_add_timer_queue(queue);
}

// Remove a timer queue from the reactor.
template <typename TimeTraits, typename Allocator>
void winrt_timer_scheduler::remove_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_remove_timer_queue(queue);
}

template <typename TimeTraits, typename Allocator>
void winrt_timer_scheduler::schedule_timer(
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
    event_.signal(lock);
}

template <typename TimeTraits, typename Allocator>
std::size_t winrt_timer_scheduler::cancel_timer(
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
void winrt_timer_scheduler::move_timer(
    timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& to,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& from)
{
  netio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  queue.cancel_timer(to, ops);
  queue.move_timer(to, from);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_IMPL_WINRT_TIMER_SCHEDULER_HPP
