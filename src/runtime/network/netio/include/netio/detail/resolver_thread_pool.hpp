#ifndef NETIO_DETAIL_RESOLVER_THREAD_POOL_HPP
#define NETIO_DETAIL_RESOLVER_THREAD_POOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/execution_context.hpp"
#include "netio/detail/mutex.hpp"
#include "netio/detail/resolve_op.hpp"
#include "netio/detail/scheduler.hpp"
#include "netio/detail/thread_group.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else // defined(NETIO_HAS_IOCP)
# include "netio/detail/scheduler.hpp"
#endif // defined(NETIO_HAS_IOCP)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class resolver_thread_pool :
  public execution_context_service_base<resolver_thread_pool>
{
public:
#if defined(NETIO_HAS_IOCP)
  typedef class win_iocp_io_context scheduler_impl;
#else
  typedef class scheduler scheduler_impl;
#endif

  // Constructor.
  NETIO_DECL resolver_thread_pool(execution_context& context);

  // Destructor.
  NETIO_DECL ~resolver_thread_pool();

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Perform any fork-related housekeeping.
  NETIO_DECL void notify_fork(execution_context::fork_event fork_ev);

  // Helper function to start an asynchronous resolve operation.
  NETIO_DECL void start_resolve_op(resolve_op* op);

  // Get the underlying scheduler implementation.
  scheduler_impl& scheduler()
  {
    return scheduler_;
  }

private:
  // Helper class to run the work scheduler in a thread.
  class work_scheduler_runner;

  // Start the work scheduler if it's not already running.
  NETIO_DECL void start_work_threads();

  // The scheduler implementation used to post completions.
  scheduler_impl& scheduler_;

  // Mutex to protect access to internal data.
  netio::detail::mutex mutex_;

  // Private scheduler used for performing asynchronous host resolution.
  scheduler_impl work_scheduler_;

  // Threads used for running the work scheduler's run loop.
  thread_group<execution_context::allocator<void>> work_threads_;

  // The number of threads used to run the work scheduler.
  unsigned int num_work_threads_;

  // Whether the scheduler locking is enabled.
  bool scheduler_locking_;

  // Whether the scheduler has been shut down.
  bool shutdown_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/resolver_thread_pool.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_RESOLVER_THREAD_POOL_HPP
