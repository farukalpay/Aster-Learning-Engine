#ifndef NETIO_DETAIL_WIN_IOCP_SOCKET_CONNECT_OP_HPP
#define NETIO_DETAIL_WIN_IOCP_SOCKET_CONNECT_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP)

#include "netio/detail/bind_handler.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/reactor_op.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class win_iocp_socket_connect_op_base : public reactor_op
{
public:
  win_iocp_socket_connect_op_base(socket_type socket, func_type complete_func)
    : reactor_op(netio::error_code(),
        &win_iocp_socket_connect_op_base::do_perform, complete_func),
      socket_(socket),
      connect_ex_(false)
  {
  }

  static status do_perform(reactor_op* base)
  {
    NETIO_ASSUME(base != 0);
    win_iocp_socket_connect_op_base* o(
        static_cast<win_iocp_socket_connect_op_base*>(base));

    return socket_ops::non_blocking_connect(
        o->socket_, o->ec_) ? done : not_done;
  }

  socket_type socket_;
  bool connect_ex_;
};

template <typename Handler, typename IoExecutor>
class win_iocp_socket_connect_op : public win_iocp_socket_connect_op_base
{
public:
  NETIO_DEFINE_HANDLER_PTR(win_iocp_socket_connect_op);

  win_iocp_socket_connect_op(socket_type socket,
      Handler& handler, const IoExecutor& io_ex)
    : win_iocp_socket_connect_op_base(socket,
        &win_iocp_socket_connect_op::do_complete),
      handler_(static_cast<Handler&&>(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const netio::error_code& result_ec,
      std::size_t /*bytes_transferred*/)
  {
    netio::error_code ec(result_ec);

    // Take ownership of the operation object.
    NETIO_ASSUME(base != 0);
    win_iocp_socket_connect_op* o(
        static_cast<win_iocp_socket_connect_op*>(base));
    ptr p = { netio::detail::addressof(o->handler_), o, o };

    if (owner)
    {
      if (o->connect_ex_)
        socket_ops::complete_iocp_connect(o->socket_, ec);
      else
        ec = o->ec_;
    }

    NETIO_HANDLER_COMPLETION((*o));

    // Take ownership of the operation's outstanding work.
    handler_work<Handler, IoExecutor> w(
        static_cast<handler_work<Handler, IoExecutor>&&>(
          o->work_));

    NETIO_ERROR_LOCATION(ec);

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder1<Handler, netio::error_code>
      handler(o->handler_, ec);
    p.h = netio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (owner)
    {
      fenced_block b(fenced_block::half);
      NETIO_HANDLER_INVOCATION_BEGIN((handler.arg1_));
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

#endif // defined(NETIO_HAS_IOCP)

#endif // NETIO_DETAIL_WIN_IOCP_SOCKET_CONNECT_OP_HPP
