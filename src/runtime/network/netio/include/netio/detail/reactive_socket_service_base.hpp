#ifndef NETIO_DETAIL_REACTIVE_SOCKET_SERVICE_BASE_HPP
#define NETIO_DETAIL_REACTIVE_SOCKET_SERVICE_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_IOCP) \
  && !defined(NETIO_WINDOWS_RUNTIME) \
  && !defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#include "netio/associated_cancellation_slot.hpp"
#include "netio/buffer.hpp"
#include "netio/cancellation_type.hpp"
#include "netio/error.hpp"
#include "netio/execution_context.hpp"
#include "netio/socket_base.hpp"
#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/reactive_null_buffers_op.hpp"
#include "netio/detail/reactive_socket_recv_op.hpp"
#include "netio/detail/reactive_socket_recvmsg_op.hpp"
#include "netio/detail/reactive_socket_send_op.hpp"
#include "netio/detail/reactive_wait_op.hpp"
#include "netio/detail/reactor.hpp"
#include "netio/detail/reactor_op.hpp"
#include "netio/detail/socket_holder.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class reactive_socket_service_base
{
public:
  // The native type of a socket.
  typedef socket_type native_handle_type;

  // The implementation type of the socket.
  struct base_implementation_type
  {
    // The native socket representation.
    socket_type socket_;

    // The current state of the socket.
    socket_ops::state_type state_;

    // Per-descriptor data used by the reactor.
    reactor::per_descriptor_data reactor_data_;
  };

  // Constructor.
  NETIO_DECL reactive_socket_service_base(execution_context& context);

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void base_shutdown();

  // Construct a new socket implementation.
  NETIO_DECL void construct(base_implementation_type& impl);

  // Move-construct a new socket implementation.
  NETIO_DECL void base_move_construct(base_implementation_type& impl,
      base_implementation_type& other_impl) noexcept;

  // Move-assign from another socket implementation.
  NETIO_DECL void base_move_assign(base_implementation_type& impl,
      reactive_socket_service_base& other_service,
      base_implementation_type& other_impl);

  // Destroy a socket implementation.
  NETIO_DECL void destroy(base_implementation_type& impl);

  // Determine whether the socket is open.
  bool is_open(const base_implementation_type& impl) const
  {
    return impl.socket_ != invalid_socket;
  }

  // Destroy a socket implementation.
  NETIO_DECL netio::error_code close(
      base_implementation_type& impl, netio::error_code& ec);

  // Release ownership of the socket.
  NETIO_DECL socket_type release(
      base_implementation_type& impl, netio::error_code& ec);

  // Get the native socket representation.
  native_handle_type native_handle(base_implementation_type& impl)
  {
    return impl.socket_;
  }

  // Cancel all operations associated with the socket.
  NETIO_DECL netio::error_code cancel(
      base_implementation_type& impl, netio::error_code& ec);

  // Determine whether the socket is at the out-of-band data mark.
  bool at_mark(const base_implementation_type& impl,
      netio::error_code& ec) const
  {
    return socket_ops::sockatmark(impl.socket_, ec);
  }

  // Determine the number of bytes available for reading.
  std::size_t available(const base_implementation_type& impl,
      netio::error_code& ec) const
  {
    return socket_ops::available(impl.socket_, ec);
  }

  // Place the socket into the state where it will listen for new connections.
  netio::error_code listen(base_implementation_type& impl,
      int backlog, netio::error_code& ec)
  {
    socket_ops::listen(impl.socket_, backlog, ec);
    return ec;
  }

  // Perform an IO control command on the socket.
  template <typename IO_Control_Command>
  netio::error_code io_control(base_implementation_type& impl,
      IO_Control_Command& command, netio::error_code& ec)
  {
    socket_ops::ioctl(impl.socket_, impl.state_, command.name(),
        static_cast<ioctl_arg_type*>(command.data()), ec);
    return ec;
  }

  // Gets the non-blocking mode of the socket.
  bool non_blocking(const base_implementation_type& impl) const
  {
    return (impl.state_ & socket_ops::user_set_non_blocking) != 0;
  }

  // Sets the non-blocking mode of the socket.
  netio::error_code non_blocking(base_implementation_type& impl,
      bool mode, netio::error_code& ec)
  {
    socket_ops::set_user_non_blocking(impl.socket_, impl.state_, mode, ec);
    return ec;
  }

  // Gets the non-blocking mode of the native socket implementation.
  bool native_non_blocking(const base_implementation_type& impl) const
  {
    return (impl.state_ & socket_ops::internal_non_blocking) != 0;
  }

  // Sets the non-blocking mode of the native socket implementation.
  netio::error_code native_non_blocking(base_implementation_type& impl,
      bool mode, netio::error_code& ec)
  {
    socket_ops::set_internal_non_blocking(impl.socket_, impl.state_, mode, ec);
    return ec;
  }

  // Wait for the socket to become ready to read, ready to write, or to have
  // pending error conditions.
  netio::error_code wait(base_implementation_type& impl,
      socket_base::wait_type w, netio::error_code& ec)
  {
    switch (w)
    {
    case socket_base::wait_read:
      socket_ops::poll_read(impl.socket_, impl.state_, -1, ec);
      break;
    case socket_base::wait_write:
      socket_ops::poll_write(impl.socket_, impl.state_, -1, ec);
      break;
    case socket_base::wait_error:
      socket_ops::poll_error(impl.socket_, impl.state_, -1, ec);
      break;
    default:
      ec = netio::error::invalid_argument;
      break;
    }

    return ec;
  }

  // Asynchronously wait for the socket to become ready to read, ready to
  // write, or to have pending error conditions.
  template <typename Handler, typename IoExecutor>
  void async_wait(base_implementation_type& impl,
      socket_base::wait_type w, Handler& handler, const IoExecutor& io_ex)
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

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_wait"));

    int op_type;
    switch (w)
    {
    case socket_base::wait_read:
      op_type = reactor::read_op;
      break;
    case socket_base::wait_write:
      op_type = reactor::write_op;
      break;
    case socket_base::wait_error:
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
            &reactor_, &impl.reactor_data_, impl.socket_, op_type);
    }

    start_op(impl, op_type, p.p, is_continuation,
        false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Send the given data to the peer.
  template <typename ConstBufferSequence>
  size_t send(base_implementation_type& impl,
      const ConstBufferSequence& buffers,
      socket_base::message_flags flags, netio::error_code& ec)
  {
    typedef buffer_sequence_adapter<netio::const_buffer,
        ConstBufferSequence> bufs_type;

    if (bufs_type::is_single_buffer)
    {
      return socket_ops::sync_send1(impl.socket_,
          impl.state_, bufs_type::first(buffers).data(),
          bufs_type::first(buffers).size(), flags, ec);
    }
    else
    {
      bufs_type bufs(buffers);
      return socket_ops::sync_send(impl.socket_, impl.state_,
          bufs.buffers(), bufs.count(), flags, bufs.all_empty(), ec);
    }
  }

  // Wait until data can be sent without blocking.
  size_t send(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, netio::error_code& ec)
  {
    // Wait for socket to become ready.
    socket_ops::poll_write(impl.socket_, impl.state_, -1, ec);

    return 0;
  }

  // Start an asynchronous send. The data being sent must be valid for the
  // lifetime of the asynchronous operation.
  template <typename ConstBufferSequence, typename Handler, typename IoExecutor>
  void async_send(base_implementation_type& impl,
      const ConstBufferSequence& buffers, socket_base::message_flags flags,
      Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_socket_send_op<
        ConstBufferSequence, Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, impl.socket_,
        impl.state_, buffers, flags, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::write_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_send"));

    start_op(impl, reactor::write_op, p.p, is_continuation, true,
        ((impl.state_ & socket_ops::stream_oriented)
          && buffer_sequence_adapter<netio::const_buffer,
            ConstBufferSequence>::all_empty(buffers)), true, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Start an asynchronous wait until data can be sent without blocking.
  template <typename Handler, typename IoExecutor>
  void async_send(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, Handler& handler, const IoExecutor& io_ex)
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
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::write_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_send(null_buffers)"));

    start_op(impl, reactor::write_op, p.p,
        is_continuation, false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Receive some data from the peer. Returns the number of bytes received.
  template <typename MutableBufferSequence>
  size_t receive(base_implementation_type& impl,
      const MutableBufferSequence& buffers,
      socket_base::message_flags flags, netio::error_code& ec)
  {
    typedef buffer_sequence_adapter<netio::mutable_buffer,
        MutableBufferSequence> bufs_type;

    if (bufs_type::is_single_buffer)
    {
      return socket_ops::sync_recv1(impl.socket_,
          impl.state_, bufs_type::first(buffers).data(),
          bufs_type::first(buffers).size(), flags, ec);
    }
    else
    {
      bufs_type bufs(buffers);
      return socket_ops::sync_recv(impl.socket_, impl.state_,
          bufs.buffers(), bufs.count(), flags, bufs.all_empty(), ec);
    }
  }

  // Wait until data can be received without blocking.
  size_t receive(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, netio::error_code& ec)
  {
    // Wait for socket to become ready.
    socket_ops::poll_read(impl.socket_, impl.state_, -1, ec);

    return 0;
  }

  // Start an asynchronous receive. The buffer for the data being received
  // must be valid for the lifetime of the asynchronous operation.
  template <typename MutableBufferSequence,
      typename Handler, typename IoExecutor>
  void async_receive(base_implementation_type& impl,
      const MutableBufferSequence& buffers, socket_base::message_flags flags,
      Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_socket_recv_op<
        MutableBufferSequence, Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, impl.socket_,
        impl.state_, buffers, flags, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_receive"));

    start_op(impl,
        (flags & socket_base::message_out_of_band)
          ? reactor::except_op : reactor::read_op,
        p.p, is_continuation,
        (flags & socket_base::message_out_of_band) == 0,
        ((impl.state_ & socket_ops::stream_oriented)
          && buffer_sequence_adapter<netio::mutable_buffer,
            MutableBufferSequence>::all_empty(buffers)), true, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Wait until data can be received without blocking.
  template <typename Handler, typename IoExecutor>
  void async_receive(base_implementation_type& impl,
      const null_buffers&, socket_base::message_flags flags,
      Handler& handler, const IoExecutor& io_ex)
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
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_receive(null_buffers)"));

    start_op(impl,
        (flags & socket_base::message_out_of_band)
          ? reactor::except_op : reactor::read_op,
        p.p, is_continuation, false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Receive some data with associated flags. Returns the number of bytes
  // received.
  template <typename MutableBufferSequence>
  size_t receive_with_flags(base_implementation_type& impl,
      const MutableBufferSequence& buffers,
      socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, netio::error_code& ec)
  {
    buffer_sequence_adapter<netio::mutable_buffer,
        MutableBufferSequence> bufs(buffers);

    return socket_ops::sync_recvmsg(impl.socket_, impl.state_,
        bufs.buffers(), bufs.count(), in_flags, out_flags, ec);
  }

  // Wait until data can be received without blocking.
  size_t receive_with_flags(base_implementation_type& impl,
      const null_buffers&, socket_base::message_flags,
      socket_base::message_flags& out_flags, netio::error_code& ec)
  {
    // Wait for socket to become ready.
    socket_ops::poll_read(impl.socket_, impl.state_, -1, ec);

    // Clear out_flags, since we cannot give it any other sensible value when
    // performing a null_buffers operation.
    out_flags = 0;

    return 0;
  }

  // Start an asynchronous receive. The buffer for the data being received
  // must be valid for the lifetime of the asynchronous operation.
  template <typename MutableBufferSequence,
      typename Handler, typename IoExecutor>
  void async_receive_with_flags(base_implementation_type& impl,
      const MutableBufferSequence& buffers, socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, Handler& handler,
      const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    associated_cancellation_slot_t<Handler> slot
      = netio::get_associated_cancellation_slot(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef reactive_socket_recvmsg_op<
        MutableBufferSequence, Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(success_ec_, impl.socket_,
        buffers, in_flags, out_flags, handler, io_ex);

    // Optionally register for per-operation cancellation.
    if (slot.is_connected())
    {
      p.p->cancellation_key_ =
        &slot.template emplace<reactor_op_cancellation>(
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_receive_with_flags"));

    start_op(impl,
        (in_flags & socket_base::message_out_of_band)
          ? reactor::except_op : reactor::read_op,
        p.p, is_continuation,
        (in_flags & socket_base::message_out_of_band) == 0,
        false, true, &io_ex, 0);
    p.v = p.p = 0;
  }

  // Wait until data can be received without blocking.
  template <typename Handler, typename IoExecutor>
  void async_receive_with_flags(base_implementation_type& impl,
      const null_buffers&, socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, Handler& handler,
      const IoExecutor& io_ex)
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
            &reactor_, &impl.reactor_data_, impl.socket_, reactor::read_op);
    }

    NETIO_HANDLER_CREATION((reactor_.context(), *p.p, "socket",
          &impl, impl.socket_, "async_receive_with_flags(null_buffers)"));

    // Clear out_flags, since we cannot give it any other sensible value when
    // performing a null_buffers operation.
    out_flags = 0;

    start_op(impl,
        (in_flags & socket_base::message_out_of_band)
          ? reactor::except_op : reactor::read_op,
        p.p, is_continuation, false, false, false, &io_ex, 0);
    p.v = p.p = 0;
  }

protected:
  // Open a new socket implementation.
  NETIO_DECL netio::error_code do_open(
      base_implementation_type& impl, int af,
      int type, int protocol, netio::error_code& ec);

  // Assign a native socket to a socket implementation.
  NETIO_DECL netio::error_code do_assign(
      base_implementation_type& impl, int type,
      const native_handle_type& native_socket, netio::error_code& ec);

  // Start the asynchronous read or write operation.
  NETIO_DECL void do_start_op(base_implementation_type& impl,
      int op_type, reactor_op* op, bool is_continuation,
      bool allow_speculative, bool noop, bool needs_non_blocking,
      void (*on_immediate)(operation* op, bool, const void*),
      const void* immediate_arg);

  // Start the asynchronous operation for handlers that are specialised for
  // immediate completion.
  template <typename Op>
  void start_op(base_implementation_type& impl, int op_type, Op* op,
      bool is_continuation, bool allow_speculative, bool noop,
      bool needs_non_blocking, const void* io_ex, ...)
  {
    return do_start_op(impl, op_type, op, is_continuation, allow_speculative,
        noop, needs_non_blocking, &Op::do_immediate, io_ex);
  }

  // Start the asynchronous operation for handlers that are not specialised for
  // immediate completion.
  template <typename Op>
  void start_op(base_implementation_type& impl, int op_type,
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

  // Start the asynchronous accept operation.
  NETIO_DECL void do_start_accept_op(base_implementation_type& impl,
      reactor_op* op, bool is_continuation, bool peer_is_open,
      void (*on_immediate)(operation* op, bool, const void*),
      const void* immediate_arg);

  // Start the asynchronous accept operation for handlers that are specialised
  // for immediate completion.
  template <typename Op>
  void start_accept_op(base_implementation_type& impl, Op* op,
      bool is_continuation, bool peer_is_open, const void* io_ex, ...)
  {
    return do_start_accept_op(impl, op, is_continuation,
        peer_is_open, &Op::do_immediate, io_ex);
  }

  // Start the asynchronous operation for handlers that are not specialised for
  // immediate completion.
  template <typename Op>
  void start_accept_op(base_implementation_type& impl, Op* op,
      bool is_continuation, bool peer_is_open, const void*,
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
    return do_start_accept_op(impl, op, is_continuation, peer_is_open,
        &reactor::call_post_immediate_completion, &reactor_);
  }

  // Start the asynchronous connect operation.
  NETIO_DECL void do_start_connect_op(base_implementation_type& impl,
      reactor_op* op, bool is_continuation, const void* addr, size_t addrlen,
      void (*on_immediate)(operation* op, bool, const void*),
      const void* immediate_arg);

  // Start the asynchronous operation for handlers that are specialised for
  // immediate completion.
  template <typename Op>
  void start_connect_op(base_implementation_type& impl,
      Op* op, bool is_continuation, const void* addr,
      size_t addrlen, const void* io_ex, ...)
  {
    return do_start_connect_op(impl, op, is_continuation,
        addr, addrlen, &Op::do_immediate, io_ex);
  }

  // Start the asynchronous operation for handlers that are not specialised for
  // immediate completion.
  template <typename Op>
  void start_connect_op(base_implementation_type& impl, Op* op,
      bool is_continuation, const void* addr, size_t addrlen, const void*,
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
    return do_start_connect_op(impl, op, is_continuation, addr,
        addrlen, &reactor::call_post_immediate_completion, &reactor_);
  }

  // Helper class used to implement per-operation cancellation
  class reactor_op_cancellation
  {
  public:
    reactor_op_cancellation(reactor* r,
        reactor::per_descriptor_data* p, socket_type d, int o)
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
    socket_type descriptor_;
    int op_type_;
  };

  // The selector that performs event demultiplexing for the service.
  reactor& reactor_;

  // Cached success value to avoid accessing category singleton.
  const netio::error_code success_ec_;

  // Extra state flags to be applied to newly opened sockets.
  socket_ops::state_type extra_state_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/reactive_socket_service_base.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // !defined(NETIO_HAS_IOCP)
       //   && !defined(NETIO_WINDOWS_RUNTIME)
       //   && !defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#endif // NETIO_DETAIL_REACTIVE_SOCKET_SERVICE_BASE_HPP
