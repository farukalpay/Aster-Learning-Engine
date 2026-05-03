#ifndef NETIO_DETAIL_IO_URING_SOCKET_CONNECT_OP_HPP
#define NETIO_DETAIL_IO_URING_SOCKET_CONNECT_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IO_URING)

#include "netio/detail/bind_handler.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/io_uring_operation.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/socket_ops.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Protocol>
class io_uring_socket_connect_op_base : public io_uring_operation
{
public:
  io_uring_socket_connect_op_base(const netio::error_code& success_ec,
      socket_type socket, const typename Protocol::endpoint& endpoint,
      func_type complete_func)
    : io_uring_operation(success_ec,
        &io_uring_socket_connect_op_base::do_prepare,
        &io_uring_socket_connect_op_base::do_perform, complete_func),
      socket_(socket),
      endpoint_(endpoint)
  {
  }

  static void do_prepare(io_uring_operation* base, ::io_uring_sqe* sqe)
  {
    NETIO_ASSUME(base != 0);
    io_uring_socket_connect_op_base* o(
        static_cast<io_uring_socket_connect_op_base*>(base));

    ::io_uring_prep_connect(sqe, o->socket_,
        static_cast<sockaddr*>(o->endpoint_.data()),
        static_cast<socklen_t>(o->endpoint_.size()));
  }

  static bool do_perform(io_uring_operation*, bool after_completion)
  {
    return after_completion;
  }

private:
  socket_type socket_;
  typename Protocol::endpoint endpoint_;
};

template <typename Protocol, typename Handler, typename IoExecutor>
class io_uring_socket_connect_op :
  public io_uring_socket_connect_op_base<Protocol>
{
public:
  NETIO_DEFINE_HANDLER_PTR(io_uring_socket_connect_op);

  io_uring_socket_connect_op(const netio::error_code& success_ec,
      socket_type socket, const typename Protocol::endpoint& endpoint,
      Handler& handler, const IoExecutor& io_ex)
    : io_uring_socket_connect_op_base<Protocol>(success_ec, socket,
        endpoint, &io_uring_socket_connect_op::do_complete),
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
    io_uring_socket_connect_op* o
      (static_cast<io_uring_socket_connect_op*>(base));
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
    detail::binder1<Handler, netio::error_code>
      handler(o->handler_, o->ec_);
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

#endif // defined(NETIO_HAS_IO_URING)

#endif // NETIO_DETAIL_IO_URING_SOCKET_CONNECT_OP_HPP
