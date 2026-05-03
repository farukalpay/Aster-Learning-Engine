#ifndef NETIO_DETAIL_COMPLETION_PAYLOAD_HANDLER_HPP
#define NETIO_DETAIL_COMPLETION_PAYLOAD_HANDLER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/associator.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Payload, typename Handler>
class completion_payload_handler
{
public:
  completion_payload_handler(Payload&& p, Handler& h)
    : payload_(static_cast<Payload&&>(p)),
      handler_(static_cast<Handler&&>(h))
  {
  }

  void operator()()
  {
    payload_.receive(handler_);
  }

  Handler& handler()
  {
    return handler_;
  }

//private:
  Payload payload_;
  Handler handler_;
};

} // namespace detail

template <template <typename, typename> class Associator,
    typename Payload, typename Handler, typename DefaultCandidate>
struct associator<Associator,
    detail::completion_payload_handler<Payload, Handler>,
    DefaultCandidate>
  : Associator<Handler, DefaultCandidate>
{
  static typename Associator<Handler, DefaultCandidate>::type get(
      const detail::completion_payload_handler<Payload, Handler>& h) noexcept
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_);
  }

  static auto get(
      const detail::completion_payload_handler<Payload, Handler>& h,
      const DefaultCandidate& c) noexcept
    -> decltype(Associator<Handler, DefaultCandidate>::get(h.handler_, c))
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_, c);
  }
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_COMPLETION_PAYLOAD_HANDLER_HPP
