#ifndef NETIO_DETAIL_WIN_IOCP_SOCKET_SEND_OP_HPP
#define NETIO_DETAIL_WIN_IOCP_SOCKET_SEND_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP)

#include "netio/detail/bind_handler.hpp"
#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/operation.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename ConstBufferSequence, typename Handler, typename IoExecutor>
class win_iocp_socket_send_op : public operation
{
public:
  NETIO_DEFINE_HANDLER_PTR(win_iocp_socket_send_op);

  win_iocp_socket_send_op(socket_ops::weak_cancel_token_type cancel_token,
      const ConstBufferSequence& buffers, Handler& handler,
      const IoExecutor& io_ex)
    : operation(&win_iocp_socket_send_op::do_complete),
      cancel_token_(cancel_token),
      buffers_(buffers),
      handler_(static_cast<Handler&&>(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const netio::error_code& result_ec,
      std::size_t bytes_transferred)
  {
    netio::error_code ec(result_ec);

    // Take ownership of the operation object.
    NETIO_ASSUME(base != 0);
    win_iocp_socket_send_op* o(static_cast<win_iocp_socket_send_op*>(base));
    ptr p = { netio::detail::addressof(o->handler_), o, o };

    NETIO_HANDLER_COMPLETION((*o));

    // Take ownership of the operation's outstanding work.
    handler_work<Handler, IoExecutor> w(
        static_cast<handler_work<Handler, IoExecutor>&&>(
          o->work_));

#if defined(NETIO_ENABLE_BUFFER_DEBUGGING)
    // Check whether buffers are still valid.
    if (owner)
    {
      buffer_sequence_adapter<netio::const_buffer,
          ConstBufferSequence>::validate(o->buffers_);
    }
#endif // defined(NETIO_ENABLE_BUFFER_DEBUGGING)

    socket_ops::complete_iocp_send(o->cancel_token_, ec);

    NETIO_ERROR_LOCATION(ec);

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder2<Handler, netio::error_code, std::size_t>
      handler(o->handler_, ec, bytes_transferred);
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
  socket_ops::weak_cancel_token_type cancel_token_;
  ConstBufferSequence buffers_;
  Handler handler_;
  handler_work<Handler, IoExecutor> work_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IOCP)

#endif // NETIO_DETAIL_WIN_IOCP_SOCKET_SEND_OP_HPP
