#ifndef NETIO_DETAIL_WINRT_TIMER_SCHEDULER_HPP
#define NETIO_DETAIL_WINRT_TIMER_SCHEDULER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)

#include <cstddef>
#include "netio/detail/event.hpp"
#include "netio/detail/limits.hpp"
#include "netio/detail/mutex.hpp"
#include "netio/detail/op_queue.hpp"
#include "netio/detail/thread.hpp"
#include "netio/detail/timer_queue_base.hpp"
#include "netio/detail/timer_queue_set.hpp"
#include "netio/detail/wait_op.hpp"
#include "netio/execution_context.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else // defined(NETIO_HAS_IOCP)
# include "netio/detail/scheduler.hpp"
#endif // defined(NETIO_HAS_IOCP)

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/thread.hpp"
#endif // defined(NETIO_HAS_IOCP)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class winrt_timer_scheduler
  : public execution_context_service_base<winrt_timer_scheduler>
{
public:
  // Constructor.
  NETIO_DECL winrt_timer_scheduler(execution_context& context);

  // Destructor.
  NETIO_DECL ~winrt_timer_scheduler();

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Recreate internal descriptors following a fork.
  NETIO_DECL void notify_fork(execution_context::fork_event fork_ev);

  // Initialise the task. No effect as this class uses its own thread.
  NETIO_DECL void init_task();

  // Add a new timer queue to the reactor.
  template <typename TimeTraits, typename Allocator>
  void add_timer_queue(timer_queue<TimeTraits, Allocator>& queue);

  // Remove a timer queue from the reactor.
  template <typename TimeTraits, typename Allocator>
  void remove_timer_queue(timer_queue<TimeTraits, Allocator>& queue);

  // Schedule a new operation in the given timer queue to expire at the
  // specified absolute time.
  template <typename TimeTraits, typename Allocator>
  void schedule_timer(timer_queue<TimeTraits, Allocator>& queue,
      const typename TimeTraits::time_type& time,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
      wait_op* op);

  // Cancel the timer operations associated with the given token. Returns the
  // number of operations that have been posted or dispatched.
  template <typename TimeTraits, typename Allocator>
  std::size_t cancel_timer(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
      std::size_t max_cancelled = (std::numeric_limits<std::size_t>::max)());

  // Move the timer operations associated with the given timer.
  template <typename TimeTraits, typename Allocator>
  void move_timer(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& to,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& from);

private:
  // Run the select loop in the thread.
  NETIO_DECL void run_thread();

  // Entry point for the select loop thread.
  NETIO_DECL static void call_run_thread(winrt_timer_scheduler* reactor);

  // Helper function to add a new timer queue.
  NETIO_DECL void do_add_timer_queue(timer_queue_base& queue);

  // Helper function to remove a timer queue.
  NETIO_DECL void do_remove_timer_queue(timer_queue_base& queue);

  // The scheduler implementation used to post completions.
#if defined(NETIO_HAS_IOCP)
  typedef class win_iocp_io_context scheduler_impl;
#else
  typedef class scheduler scheduler_impl;
#endif
  scheduler_impl& scheduler_;

  // Mutex used to protect internal variables.
  netio::detail::mutex mutex_;

  // Event used to wake up background thread.
  netio::detail::event event_;

  // The timer queues.
  timer_queue_set timer_queues_;

  // The background thread that is waiting for timers to expire.
  netio::detail::thread thread_;

  // Does the background thread need to stop.
  bool stop_thread_;

  // Whether the service has been shut down.
  bool shutdown_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/detail/impl/winrt_timer_scheduler.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/winrt_timer_scheduler.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_WINRT_TIMER_SCHEDULER_HPP
