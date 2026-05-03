#ifndef NETIO_DETAIL_WAIT_HANDLER_HPP
#define NETIO_DETAIL_WAIT_HANDLER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/bind_handler.hpp"
#include "netio/detail/fenced_block.hpp"
#include "netio/detail/handler_alloc_helpers.hpp"
#include "netio/detail/handler_work.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/wait_op.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Handler, typename IoExecutor>
class wait_handler : public wait_op
{
public:
  NETIO_DEFINE_HANDLER_PTR(wait_handler);

  wait_handler(Handler& h, const IoExecutor& io_ex)
    : wait_op(&wait_handler::do_complete),
      handler_(static_cast<Handler&&>(h)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const netio::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the handler object.
    wait_handler* h(static_cast<wait_handler*>(base));
    ptr p = { netio::detail::addressof(h->handler_), h, h };

    NETIO_HANDLER_COMPLETION((*h));

    // Take ownership of the operation's outstanding work.
    handler_work<Handler, IoExecutor> w(
        static_cast<handler_work<Handler, IoExecutor>&&>(
          h->work_));

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder1<Handler, netio::error_code>
      handler(h->handler_, h->ec_);
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

#endif // NETIO_DETAIL_WAIT_HANDLER_HPP
