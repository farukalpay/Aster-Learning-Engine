#ifndef NETIO_DETAIL_WINRT_SOCKET_SEND_OP_HPP
#define NETIO_DETAIL_WINRT_SOCKET_SEND_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)

#include "netio/detail/bind_handler.hpp"
#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/winrt_async_op.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename ConstBufferSequence, typename Handler, typename IoExecutor>
class winrt_socket_send_op :
  public winrt_async_op<unsigned int>
{
public:
  NETIO_DEFINE_HANDLER_PTR(winrt_socket_send_op);

  winrt_socket_send_op(const ConstBufferSequence& buffers,
      Handler& handler, const IoExecutor& io_ex)
    : winrt_async_op<unsigned int>(&winrt_socket_send_op::do_complete),
      buffers_(buffers),
      handler_(static_cast<Handler&&>(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const netio::error_code&, std::size_t)
  {
    // Take ownership of the operation object.
    NETIO_ASSUME(base != 0);
    winrt_socket_send_op* o(static_cast<winrt_socket_send_op*>(base));
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

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder2<Handler, netio::error_code, std::size_t>
      handler(o->handler_, o->ec_, o->result_);
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
  ConstBufferSequence buffers_;
  Handler handler_;
  handler_work<Handler, IoExecutor> executor_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_WINRT_SOCKET_SEND_OP_HPP
