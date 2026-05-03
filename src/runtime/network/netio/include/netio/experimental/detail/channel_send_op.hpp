#ifndef NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_OP_HPP
#define NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/bind_handler.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/error.hpp"
#include "netio/experimental/channel_error.hpp"
#include "netio/experimental/detail/channel_operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace detail {

template <typename Payload>
class channel_send : public channel_operation
{
public:
  Payload get_payload()
  {
    return static_cast<Payload&&>(payload_);
  }

  void immediate()
  {
    func_(this, immediate_op, 0);
  }

  void post()
  {
    func_(this, post_op, 0);
  }

  void cancel()
  {
    func_(this, cancel_op, 0);
  }

  void close()
  {
    func_(this, close_op, 0);
  }

protected:
  channel_send(func_type func, Payload&& payload)
    : channel_operation(func),
      payload_(static_cast<Payload&&>(payload))
  {
  }

private:
  Payload payload_;
};

template <typename Payload, typename Handler, typename IoExecutor>
class channel_send_op : public channel_send<Payload>
{
public:
  NETIO_DEFINE_HANDLER_PTR(channel_send_op);

  channel_send_op(Payload&& payload,
      Handler& handler, const IoExecutor& io_ex)
    : channel_send<Payload>(&channel_send_op::do_action,
        static_cast<Payload&&>(payload)),
      handler_(static_cast<Handler&&>(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_action(channel_operation* base,
      channel_operation::action a, void*)
  {
    // Take ownership of the operation object.
    channel_send_op* o(static_cast<channel_send_op*>(base));
    ptr p = { netio::detail::addressof(o->handler_), o, o };

    NETIO_HANDLER_COMPLETION((*o));

    // Take ownership of the operation's outstanding work.
    channel_operation::handler_work<Handler, IoExecutor> w(
        static_cast<channel_operation::handler_work<Handler, IoExecutor>&&>(
          o->work_));

    netio::error_code ec;
    switch (a)
    {
    case channel_operation::cancel_op:
      ec = error::channel_cancelled;
      break;
    case channel_operation::close_op:
      ec = error::channel_closed;
      break;
    default:
      break;
    }

    // Make a copy of the handler so that the memory can be deallocated before
    // the handler is posted. Even if we're not about to post the handler, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    netio::detail::binder1<Handler, netio::error_code>
      handler(o->handler_, ec);
    p.h = netio::detail::addressof(handler.handler_);
    p.reset();

    // Post the completion if required.
    if (a != channel_operation::destroy_op)
    {
      NETIO_HANDLER_INVOCATION_BEGIN((handler.arg1_));
      if (a == channel_operation::immediate_op)
        w.immediate(handler, handler.handler_, 0);
      else
        w.post(handler, handler.handler_);
      NETIO_HANDLER_INVOCATION_END;
    }
  }

private:
  Handler handler_;
  channel_operation::handler_work<Handler, IoExecutor> work_;
};

} // namespace detail
} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_OP_HPP
