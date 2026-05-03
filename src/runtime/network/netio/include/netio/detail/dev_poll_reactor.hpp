#ifndef NETIO_DETAIL_DEV_POLL_REACTOR_HPP
#define NETIO_DETAIL_DEV_POLL_REACTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_DEV_POLL)

#include <cstddef>
#include <vector>
#include <sys/devpoll.h>
#include "netio/detail/hash_map.hpp"
#include "netio/detail/limits.hpp"
#include "netio/detail/mutex.hpp"
#include "netio/detail/op_queue.hpp"
#include "netio/detail/reactor_op.hpp"
#include "netio/detail/reactor_op_queue.hpp"
#include "netio/detail/scheduler_task.hpp"
#include "netio/detail/select_interrupter.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/detail/timer_queue_base.hpp"
#include "netio/detail/timer_queue_set.hpp"
#include "netio/detail/wait_op.hpp"
#include "netio/execution_context.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class dev_poll_reactor
  : public execution_context_service_base<dev_poll_reactor>,
    public scheduler_task
{
public:
  enum op_types { read_op = 0, write_op = 1,
    connect_op = 1, except_op = 2, max_ops = 3 };

  // Per-descriptor data.
  struct per_descriptor_data
  {
  };

  // Constructor.
  NETIO_DECL dev_poll_reactor(netio::execution_context& ctx);

  // Destructor.
  NETIO_DECL ~dev_poll_reactor();

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Recreate internal descriptors following a fork.
  NETIO_DECL void notify_fork(
      netio::execution_context::fork_event fork_ev);

  // Initialise the task.
  NETIO_DECL void init_task();

  // Register a socket with the reactor. Returns 0 on success, system error
  // code on failure.
  NETIO_DECL int register_descriptor(socket_type, per_descriptor_data&);

  // Register a descriptor with an associated single operation. Returns 0 on
  // success, system error code on failure.
  NETIO_DECL int register_internal_descriptor(
      int op_type, socket_type descriptor,
      per_descriptor_data& descriptor_data, reactor_op* op);

  // Move descriptor registration from one descriptor_data object to another.
  NETIO_DECL void move_descriptor(socket_type descriptor,
      per_descriptor_data& target_descriptor_data,
      per_descriptor_data& source_descriptor_data);

  // Post a reactor operation for immediate completion.
  void post_immediate_completion(operation* op, bool is_continuation) const;

  // Post a reactor operation for immediate completion.
  NETIO_DECL static void call_post_immediate_completion(
      operation* op, bool is_continuation, const void* self);

  // Start a new operation. The reactor operation will be performed when the
  // given descriptor is flagged as ready, or an error has occurred.
  NETIO_DECL void start_op(int op_type, socket_type descriptor,
      per_descriptor_data&, reactor_op* op,
      bool is_continuation, bool allow_speculative,
      void (*on_immediate)(operation*, bool, const void*),
      const void* immediate_arg);

  // Start a new operation. The reactor operation will be performed when the
  // given descriptor is flagged as ready, or an error has occurred.
  void start_op(int op_type, socket_type descriptor,
      per_descriptor_data& descriptor_data, reactor_op* op,
      bool is_continuation, bool allow_speculative)
  {
    start_op(op_type, descriptor, descriptor_data,
        op, is_continuation, allow_speculative,
        &dev_poll_reactor::call_post_immediate_completion, this);
  }

  // Cancel all operations associated with the given descriptor. The
  // handlers associated with the descriptor will be invoked with the
  // operation_aborted error.
  NETIO_DECL void cancel_ops(socket_type descriptor, per_descriptor_data&);

  // Cancel all operations associated with the given descriptor and key. The
  // handlers associated with the descriptor will be invoked with the
  // operation_aborted error.
  NETIO_DECL void cancel_ops_by_key(socket_type descriptor,
      per_descriptor_data& descriptor_data,
      int op_type, void* cancellation_key);

  // Cancel any operations that are running against the descriptor and remove
  // its registration from the reactor. The reactor resources associated with
  // the descriptor must be released by calling cleanup_descriptor_data.
  NETIO_DECL void deregister_descriptor(socket_type descriptor,
      per_descriptor_data&, bool closing);

  // Remove the descriptor's registration from the reactor. The reactor
  // resources associated with the descriptor must be released by calling
  // cleanup_descriptor_data.
  NETIO_DECL void deregister_internal_descriptor(
      socket_type descriptor, per_descriptor_data&);

  // Perform any post-deregistration cleanup tasks associated with the
  // descriptor data.
  NETIO_DECL void cleanup_descriptor_data(per_descriptor_data&);

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

  // Cancel the timer operations associated with the given key.
  template <typename TimeTraits, typename Allocator>
  void cancel_timer_by_key(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data* timer,
      void* cancellation_key);

  // Move the timer operations associated with the given timer.
  template <typename TimeTraits, typename Allocator>
  void move_timer(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& target,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& source);

  // Run /dev/poll once until interrupted or events are ready to be dispatched.
  NETIO_DECL void run(long usec, op_queue<operation>& ops);

  // Interrupt the select loop.
  NETIO_DECL void interrupt();

private:
  // Create the /dev/poll file descriptor. Throws an exception if the descriptor
  // cannot be created.
  NETIO_DECL static int do_dev_poll_create();

  // Helper function to add a new timer queue.
  NETIO_DECL void do_add_timer_queue(timer_queue_base& queue);

  // Helper function to remove a timer queue.
  NETIO_DECL void do_remove_timer_queue(timer_queue_base& queue);

  // Get the timeout value for the /dev/poll DP_POLL operation. The timeout
  // value is returned as a number of milliseconds. A return value of -1
  // indicates that the poll should block indefinitely.
  NETIO_DECL int get_timeout(int msec);

  // Cancel all operations associated with the given descriptor. The do_cancel
  // function of the handler objects will be invoked. This function does not
  // acquire the dev_poll_reactor's mutex.
  NETIO_DECL void cancel_ops_unlocked(socket_type descriptor,
      const netio::error_code& ec);

  // Add a pending event entry for the given descriptor.
  NETIO_DECL ::pollfd& add_pending_event_change(int descriptor);

  // The scheduler implementation used to post completions.
  scheduler& scheduler_;

  // Mutex to protect access to internal data.
  netio::detail::mutex mutex_;

  // The /dev/poll file descriptor.
  int dev_poll_fd_;

  // Vector of /dev/poll events waiting to be written to the descriptor.
  std::vector< ::pollfd> pending_event_changes_;

  // Hash map to associate a descriptor with a pending event change index.
  hash_map<int, std::size_t> pending_event_change_index_;

  // The interrupter is used to break a blocking DP_POLL operation.
  select_interrupter interrupter_;

  // The queues of read, write and except operations.
  reactor_op_queue<socket_type> op_queue_[max_ops];

  // The timer queues.
  timer_queue_set timer_queues_;

  // Whether the service has been shut down.
  bool shutdown_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/detail/impl/dev_poll_reactor.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/dev_poll_reactor.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_DEV_POLL)

#endif // NETIO_DETAIL_DEV_POLL_REACTOR_HPP
