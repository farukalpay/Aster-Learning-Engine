#ifndef NETIO_DETAIL_IO_URING_SOCKET_RECVMSG_OP_HPP
#define NETIO_DETAIL_IO_URING_SOCKET_RECVMSG_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IO_URING)

#include "netio/detail/bind_handler.hpp"
#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/io_uring_operation.hpp"
#include "netio/detail/memory.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename MutableBufferSequence>
class io_uring_socket_recvmsg_op_base : public io_uring_operation
{
public:
  io_uring_socket_recvmsg_op_base(const netio::error_code& success_ec,
      socket_type socket, socket_ops::state_type state,
      const MutableBufferSequence& buffers, socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, func_type complete_func)
    : io_uring_operation(success_ec,
        &io_uring_socket_recvmsg_op_base::do_prepare,
        &io_uring_socket_recvmsg_op_base::do_perform, complete_func),
      socket_(socket),
      state_(state),
      buffers_(buffers),
      in_flags_(in_flags),
      out_flags_(out_flags),
      bufs_(buffers),
      msghdr_()
  {
    msghdr_.msg_iov = bufs_.buffers();
    msghdr_.msg_iovlen = static_cast<int>(bufs_.count());
  }

  static void do_prepare(io_uring_operation* base, ::io_uring_sqe* sqe)
  {
    NETIO_ASSUME(base != 0);
    io_uring_socket_recvmsg_op_base* o(
        static_cast<io_uring_socket_recvmsg_op_base*>(base));

    if ((o->state_ & socket_ops::internal_non_blocking) != 0)
    {
      bool except_op = (o->in_flags_ & socket_base::message_out_of_band) != 0;
      ::io_uring_prep_poll_add(sqe, o->socket_, except_op ? POLLPRI : POLLIN);
    }
    else
    {
      ::io_uring_prep_recvmsg(sqe, o->socket_, &o->msghdr_, o->in_flags_);
    }
  }

  static bool do_perform(io_uring_operation* base, bool after_completion)
  {
    NETIO_ASSUME(base != 0);
    io_uring_socket_recvmsg_op_base* o(
        static_cast<io_uring_socket_recvmsg_op_base*>(base));

    if ((o->state_ & socket_ops::internal_non_blocking) != 0)
    {
      bool except_op = (o->in_flags_ & socket_base::message_out_of_band) != 0;
      if (after_completion || !except_op)
      {
        return socket_ops::non_blocking_recvmsg(o->socket_,
            o->bufs_.buffers(), o->bufs_.count(), o->in_flags_,
            o->out_flags_, o->ec_, o->bytes_transferred_);
      }
    }
    else if (after_completion)
    {
      if (!o->ec_)
        o->out_flags_ = o->msghdr_.msg_flags;
      else
        o->out_flags_ = 0;
    }

    if (o->ec_ && o->ec_ == netio::error::would_block)
    {
      o->state_ |= socket_ops::internal_non_blocking;
      return false;
    }

    return after_completion;
  }

private:
  socket_type socket_;
  socket_ops::state_type state_;
  MutableBufferSequence buffers_;
  socket_base::message_flags in_flags_;
  socket_base::message_flags& out_flags_;
  buffer_sequence_adapter<netio::mutable_buffer,
      MutableBufferSequence> bufs_;
  msghdr msghdr_;
};

template <typename MutableBufferSequence, typename Handler, typename IoExecutor>
class io_uring_socket_recvmsg_op
  : public io_uring_socket_recvmsg_op_base<MutableBufferSequence>
{
public:
  NETIO_DEFINE_HANDLER_PTR(io_uring_socket_recvmsg_op);

  io_uring_socket_recvmsg_op(const netio::error_code& success_ec,
      int socket, socket_ops::state_type state,
      const MutableBufferSequence& buffers,
      socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags,
      Handler& handler, const IoExecutor& io_ex)
    : io_uring_socket_recvmsg_op_base<MutableBufferSequence>(success_ec,
        socket, state, buffers, in_flags, out_flags,
        &io_uring_socket_recvmsg_op::do_complete),
      handler_(static_cast<Handler&&>(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const netio::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the handler object.
    NETIO_ASSUME(base != 0);
    io_uring_socket_recvmsg_op* o
      (static_cast<io_uring_socket_recvmsg_op*>(base));
    ptr p = { netio::detail::addressof(o->handler_), o, o };

    NETIO_HANDLER_COMPLETION((*o));

    // Take ownership of the operation's outstanding work.
    handler_work<Handler, IoExecutor> w(
        static_cast<handler_work<Handler, IoExecutor>&&>(
          o->work_));

    NETIO_ERROR_LOCATION(o->ec_);

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder2<Handler, netio::error_code, std::size_t>
      handler(o->handler_, o->ec_, o->bytes_transferred_);
    p.h = netio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (owner)
    {
      fenced_block b(fenced_block::half);
      NETIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, handler.arg2_));
      w.complete(handler, handler.handler_);
      NETIO_HANDLER_INVOCATION_END;
    }
  }

private:
  Handler handler_;
  handler_work<Handler, IoExecutor> work_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IO_URING)

#endif // NETIO_DETAIL_IO_URING_SOCKET_RECVMSG_OP_HPP
