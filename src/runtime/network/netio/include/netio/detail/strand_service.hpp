#ifndef NETIO_DETAIL_STRAND_SERVICE_HPP
#define NETIO_DETAIL_STRAND_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/io_context.hpp"
#include "netio/detail/mutex.hpp"
#include "netio/detail/op_queue.hpp"
#include "netio/detail/operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

// Default service implementation for a strand.
class strand_service
  : public netio::detail::service_base<strand_service>
{
private:
  // Helper class to re-post the strand on exit.
  struct on_do_complete_exit;

  // Helper class to re-post the strand on exit.
  struct on_dispatch_exit;

public:

  // The underlying implementation of a strand.
  class strand_impl
    : public operation
  {
  public:
    strand_impl();

  private:
    // Only this service will have access to the internal values.
    friend class strand_service;
    friend struct on_do_complete_exit;
    friend struct on_dispatch_exit;

    // Mutex to protect access to internal data.
    netio::detail::mutex mutex_;

    // Indicates whether the strand is currently "locked" by a handler. This
    // means that there is a handler upcall in progress, or that the strand
    // itself has been scheduled in order to invoke some pending handlers.
    bool locked_;

    // The handlers that are waiting on the strand but should not be run until
    // after the next time the strand is scheduled. This queue must only be
    // modified while the mutex is locked.
    op_queue<operation> waiting_queue_;

    // The handlers that are ready to be run. Logically speaking, these are the
    // handlers that hold the strand's lock. The ready queue is only modified
    // from within the strand and so may be accessed without locking the mutex.
    op_queue<operation> ready_queue_;
  };

  typedef strand_impl* implementation_type;

  // Construct a new strand service for the specified io_context.
  NETIO_DECL explicit strand_service(netio::io_context& io_context);

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Construct a new strand implementation.
  NETIO_DECL void construct(implementation_type& impl);

  // Request the io_context to invoke the given handler.
  template <typename Handler>
  void dispatch(implementation_type& impl, Handler& handler);

  // Request the io_context to invoke the given handler and return immediately.
  template <typename Handler>
  void post(implementation_type& impl, Handler& handler);

  // Determine whether the strand is running in the current thread.
  NETIO_DECL bool running_in_this_thread(
      const implementation_type& impl) const;

private:
  // Helper function to dispatch a handler.
  NETIO_DECL void do_dispatch(implementation_type& impl, operation* op);

  // Helper function to post a handler.
  NETIO_DECL void do_post(implementation_type& impl,
      operation* op, bool is_continuation);

  NETIO_DECL static void do_complete(void* owner,
      operation* base, const netio::error_code& ec,
      std::size_t bytes_transferred);

  // The io_context used to obtain an I/O executor.
  io_context& io_context_;

  // The io_context implementation used to post completions.
  io_context_impl& io_context_impl_;

  // Mutex to protect access to the array of implementations.
  netio::detail::mutex mutex_;

  // Number of implementations shared between all strand objects.
#if defined(NETIO_STRAND_IMPLEMENTATIONS)
  enum { num_implementations = NETIO_STRAND_IMPLEMENTATIONS };
#else // defined(NETIO_STRAND_IMPLEMENTATIONS)
  enum { num_implementations = 193 };
#endif // defined(NETIO_STRAND_IMPLEMENTATIONS)

  // Pool of implementations.
  shared_ptr<strand_impl> implementations_[num_implementations];

  // Extra value used when hashing to prevent recycled memory locations from
  // getting the same strand implementation.
  std::size_t salt_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/detail/impl/strand_service.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/strand_service.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_STRAND_SERVICE_HPP
