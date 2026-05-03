#ifndef NETIO_DETAIL_REACTIVE_DESCRIPTOR_SERVICE_HPP
#define NETIO_DETAIL_REACTIVE_DESCRIPTOR_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS) \
  && !defined(NETIO_WINDOWS_RUNTIME) \
  && !defined(__CYGWIN__) \
  && !defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#include "netio/associated_cancellation_slot.hpp"
#include "netio/associated_immediate_executor.hpp"
#include "netio/buffer.hpp"
#include "netio/cancellation_type.hpp"
#include "netio/execution_context.hpp"
#include "netio/detail/bind_handler.hpp"
#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/detail/descriptor_ops.hpp"
#include "netio/detail/descriptor_read_op.hpp"
#include "netio/detail/descriptor_write_op.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/reactive_null_buffers_op.hpp"
#include "netio/detail/reactive_wait_op.hpp"
#include "netio/detail/reactor.hpp"
#include "netio/posix/descriptor_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class reactive_descriptor_service :
  public execution_context_service_base<reactive_descriptor_service>
{
public:
  // The native type of a descriptor.
  typedef int native_handle_type;

  // The implementation type of the descriptor.
  class implementation_type
    : private netio::detail::noncopyable
  {
  public:
    // Default constructor.
    implementation_type()
      : descriptor_(-1),
        state_(0)
    {
    }

  private:
    // Only this service will have access to the internal values.
    friend class reactive_descriptor_service;

    // The native descriptor representation.
    int descriptor_;

    // The current state of the descriptor.
    descriptor_ops::state_type state_;

    // Per-descriptor data used by the reactor.
    reactor::per_descriptor_data reactor_data_;
  };

  // Constructor.
  NETIO_DECL reactive_descriptor_service(execution_context& context);

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Construct a new descriptor implementation.
  NETIO_DECL void construct(implementation_type& impl);

  // Move-construct a new descriptor implementation.
  NETIO_DECL void move_construct(implementation_type& impl,
      implementation_type& other_impl) noexcept;

  // Move-assign from another descriptor implementation.
  NETIO_DECL void move_assign(implementation_type& impl,
      reactive_descriptor_service& other_service,
      implementation_type& other_impl);

  // Destroy a descriptor implementation.
  NETIO_DECL void destroy(implementation_type& impl);

  // Assign a native descriptor to a descriptor implementation.
  NETIO_DECL netio::error_code assign(implementation_type& impl,
      const native_handle_type& native_descriptor,
      netio::error_code& ec);

  // Determine whether the descriptor is open.
  bool is_open(const implementation_type& impl) const
  {
    return impl.descriptor_ != -1;
  }

  // Destroy a descriptor implementation.
  NETIO_DECL netio::error_code close(implementation_type& impl,
      netio::error_code& ec);

  // Get the native descriptor representation.
  native_handle_type native_handle(const implementation_type& impl) const
  {
    return impl.descriptor_;
  }

  // Release ownership of the native descriptor representation.
  NETIO_DECL native_handle_type release(implementation_type& impl);

  // Release ownership of the native descriptor representation.
  native_handle_type release(implementation_type& impl,
      netio::error_code& ec)
  {
    ec = success_ec_;
    return release(impl);
  }

  // Cancel all operations associated with the descriptor.
  NETIO_DECL netio::error_code cancel(implementation_type& impl,
      netio::error_code& ec);

  // Perform an IO control command on the descriptor.
  template <typename IO_Control_Command>
  netio::error_code io_control(implementation_type& impl,
      IO_Control_Command& command, netio::error_code& ec)
  {
    descriptor_ops::ioctl(impl.descriptor_, impl.state_,
        command.name(), static_cast<ioctl_arg_type*>(command.data()), ec);
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Gets the non-blocking mode of the descriptor.
  bool non_blocking(const implementation_type& impl) const
  {
    return (impl.state_ & descriptor_ops::user_set_non_blocking) != 0;
  }

  // Sets the non-blocking mode of the descriptor.
  netio::error_code non_blocking(implementation_type& impl,
      bool mode, netio::error_code& ec)
  {
    descriptor_ops::set_user_non_blocking(
        impl.descriptor_, impl.state_, mode, ec);
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Gets the non-blocking mode of the native descriptor implementation.
  bool native_non_blocking(const implementation_type& impl) const
  {
    return (impl.state_ & descriptor_ops::internal_non_blocking) != 0;
  }

  // Sets the non-blocking mode of the native descriptor implementation.
  netio::error_code native_non_blocking(implementation_type& impl,
      bool mode, netio::error_code& ec)
  {
    descriptor_ops::set_internal_non_blocking(
        impl.descriptor_, impl.state_, mode, ec);
    return ec;
  }

  // Wait for the descriptor to become ready to read, ready to write, or to have
  // pending error conditions.
  netio::error_code wait(implementation_type& impl,
      posix::descriptor_base::wait_type w, netio::error_code& ec)
  {
    switch (w)
    {
    case posix::descriptor_base::wait_read:
      descriptor_ops::poll_read(impl.descriptor_, impl.state_, ec);
      break;
    case posix::descriptor_base::wait_write:
      descriptor_ops::poll_write(impl.descriptor_, impl.state_, ec);
      break;
    case posix::descriptor_base::wait_error:
      descriptor_ops::poll_error(impl.descriptor_, impl.state_, ec);
      break;
    default:
      ec = netio::error::invalid_argument;
      break;
    }

    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Asynchronously wait for the descriptor to become ready to read, ready to
  // write, or to have pending error conditions.
  template <typename Handler, typename IoExecutor>
  void async_wait(implementation_type& impl,
      posix::descriptor_base::wait_type w,
      Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_wait_op<Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, handler, io_ex);

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "descriptor",
          &impl, impl.descriptor_, "async_wait"));

    int op_type;
    switch (w)
    {
    case posix::descriptor_base::wait_read:
      op_type = reactor::read_op;
      break;
    case posix::descriptor_base::wait_write:
      op_type = reactor::write_op;
      break;
    case posix::descriptor_base::wait_error:
      op_type = reactor::except_op;
      break;
    default:
      p.p->ec_ = netio::error::invalid_argument;
      start_op(impl, reactor::read_op, p.p,
          is_continuation, false, true, false, &io_ex, 0);
      p.v = p.p = 0;
      return;
    }

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_, impl.descriptor_, op_type);
    }

    start_op(impl, op_type, p.p, is_continuation,
        false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Write some data to the descriptor.
  template <typename ConstBufferSequence>
  size_t write_some(implementation_type& impl,
      const ConstBufferSequence& buffers, netio::error_code& ec)
  {
    typedef buffer_sequence_adapter<netio::const_buffer,
        ConstBufferSequence> bufs_type;

    size_t n;
    if (bufs_type::is_single_buffer)
    {
      n = descriptor_ops::sync_write1(impl.descriptor_,
          impl.state_, bufs_type::first(buffers).data(),
          bufs_type::first(buffers).size(), ec);
    }
    else
    {
      bufs_type bufs(buffers);

      n = descriptor_ops::sync_write(impl.descriptor_, impl.state_,
          bufs.buffers(), bufs.count(), bufs.all_empty(), ec);
    }

    NETIO_ERROR_LOCATION(ec);
    return n;
  }

  // Wait until data can be written without blocking.
  size_t write_some(implementation_type& impl,
      const null_buffers&, netio::error_code& ec)
  {
    // Wait for descriptor to become ready.
    descriptor_ops::poll_write(impl.descriptor_, impl.state_, ec);
    NETIO_ERROR_LOCATION(ec);
    return 0;
  }

  // Start an asynchronous write. The data being sent must be valid for the
  // lifetime of the asynchronous operation.
  template <typename ConstBufferSequence, typename Handler, typename IoExecutor>
  void async_write_some(implementation_type& impl,
      const ConstBufferSequence& buffers, Handler& handler,
      const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef descriptor_write_op<ConstBufferSequence, Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, impl.descriptor_, buffers, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_,
            impl.descriptor_, reactor::write_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "descriptor",
          &impl, impl.descriptor_, "async_write_some"));

    start_op(impl, reactor::write_op, p.p, is_continuation, true,
        buffer_sequence_adapter<netio::const_buffer,
          ConstBufferSequence>::all_empty(buffers), true, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Start an asynchronous wait until data can be written without blocking.
  template <typename Handler, typename IoExecutor>
  void async_write_some(implementation_type& impl,
      const null_buffers&, Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_null_buffers_op<Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_,
            impl.descriptor_, reactor::write_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "descriptor",
          &impl, impl.descriptor_, "async_write_some(null_buffers)"));

    start_op(impl, reactor::write_op, p.p,
        is_continuation, false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Read some data from the stream. Returns the number of bytes read.
  template <typename MutableBufferSequence>
  size_t read_some(implementation_type& impl,
      const MutableBufferSequence& buffers, netio::error_code& ec)
  {
    typedef buffer_sequence_adapter<netio::mutable_buffer,
        MutableBufferSequence> bufs_type;

    size_t n;
    if (bufs_type::is_single_buffer)
    {
      n = descriptor_ops::sync_read1(impl.descriptor_,
          impl.state_, bufs_type::first(buffers).data(),
          bufs_type::first(buffers).size(), ec);
    }
    else
    {
      bufs_type bufs(buffers);

      n = descriptor_ops::sync_read(impl.descriptor_, impl.state_,
          bufs.buffers(), bufs.count(), bufs.all_empty(), ec);
    }

    NETIO_ERROR_LOCATION(ec);
    return n;
  }

  // Wait until data can be read without blocking.
  size_t read_some(implementation_type& impl,
      const null_buffers&, netio::error_code& ec)
  {
    // Wait for descriptor to become ready.
    descriptor_ops::poll_read(impl.descriptor_, impl.state_, ec);
    NETIO_ERROR_LOCATION(ec);
    return 0;
  }

  // Start an asynchronous read. The buffer for the data being read must be
  // valid for the lifetime of the asynchronous operation.
  template <typename MutableBufferSequence,
      typename Handler, typename IoExecutor>
  void async_read_some(implementation_type& impl,
      const MutableBufferSequence& buffers,
      Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef descriptor_read_op<MutableBufferSequence, Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, impl.descriptor_, buffers, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_,
            impl.descriptor_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "descriptor",
          &impl, impl.descriptor_, "async_read_some"));

    start_op(impl, reactor::read_op, p.p, is_continuation, true,
        buffer_sequence_adapter<netio::mutable_buffer,
          MutableBufferSequence>::all_empty(buffers), true, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Wait until data can be read without blocking.
  template <typename Handler, typename IoExecutor>
  void async_read_some(implementation_type& impl,
      const null_buffers&, Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_null_buffers_op<Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_,
            impl.descriptor_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "descriptor",
          &impl, impl.descriptor_, "async_read_some(null_buffers)"));

    start_op(impl, reactor::read_op, p.p,
        is_continuation, false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

private:
  // Start the asynchronous operation.
  NETIO_DECL void do_start_op(implementation_type& impl,
      int op_type, reactor_op* op, bool is_continuation,
      bool allow_speculative, bool noop, bool needs_non_blocking,
      void (*on_immediate)(operation* op, bool, const void*),
      const void* immediate_arg);

  // Start the asynchronous operation for handlers that are specialised for
  // immediate completion.
  template <typename Op>
  void start_op(implementation_type& impl, int op_type, Op* op,
      bool is_continuation, bool allow_speculative, bool noop,
      bool needs_non_blocking, const void* io_ex, ...)
  {
    return do_start_op(impl, op_type, op, is_continuation, allow_speculative,
        noop, needs_non_blocking, &Op::do_immediate, io_ex);
  }

  // Start the asynchronous operation for handlers that are not specialised for
  // immediate completion.
  template <typename Op>
  void start_op(implementation_type& impl, int op_type,
      Op* op, bool is_continuation, bool allow_speculative,
      bool noop, bool needs_non_blocking, const void*,
      enable_if_t<
        is_same<
          typename associated_immediate_executor<
            typename Op::handler_type,
            typename Op::io_executor_type
          >::netio_associated_immediate_executor_is_unspecialised,
          void
        >::value
      >*)
  {
    return do_start_op(impl, op_type, op, is_continuation,
        allow_speculative, noop, needs_non_blocking,
        &reactor::call_post_immediate_completion, &reactor_);
  }

  // Helper class used to implement per-operation cancellation
  class reactor_op_cancellation
  {
  public:
    reactor_op_cancellation(reactor* r,
        reactor::per_descriptor_data* p, int d, int o)
      : reactor_(r),
        reactor_data_(p),
        descriptor_(d),
        op_type_(o)
    {
    }

    void operator()(cancellation_type_t type)
    {
      if (!!(type &
            (cancellation_type::terminal
              | cancellation_type::partial
              | cancellation_type::total)))
      {
        reactor_->cancel_ops_by_key(descriptor_,
            *reactor_data_, op_type_, this);
      }
    }

  private:
    reactor* reactor_;
    reactor::per_descriptor_data* reactor_data_;
    int descriptor_;
    int op_type_;
  };

  // The selector that performs event demultiplexing for the service.
  reactor& reactor_;

  // Cached success value to avoid accessing category singleton.
  const netio::error_code success_ec_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/reactive_descriptor_service.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // !defined(NETIO_WINDOWS)
       //   && !defined(NETIO_WINDOWS_RUNTIME)
       //   && !defined(__CYGWIN__)
       //   && !defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#endif // NETIO_DETAIL_REACTIVE_DESCRIPTOR_SERVICE_HPP
