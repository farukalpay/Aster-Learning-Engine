#ifndef NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_FUNCTIONS_HPP
#define NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_FUNCTIONS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/async_result.hpp"
#include "netio/detail/completion_message.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/error_code.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace detail {

template <typename Derived, typename Executor, typename... Signatures>
class channel_send_functions;

template <typename Derived, typename Executor, typename R, typename... Args>
class channel_send_functions<Derived, Executor, R(Args...)>
{
public:
  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    bool
  > try_send(Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send<message_type>(
        self->impl_, false, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    bool
  > try_send_via_dispatch(Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send<message_type>(
        self->impl_, true, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    std::size_t
  > try_send_n(std::size_t count, Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send_n<message_type>(
        self->impl_, count, false, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    std::size_t
  > try_send_n_via_dispatch(std::size_t count, Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send_n<message_type>(
        self->impl_, count, true, static_cast<Args2&&>(args)...);
  }

  template <
      NETIO_COMPLETION_TOKEN_FOR(void (netio::error_code))
        CompletionToken NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
  auto async_send(Args... args,
      CompletionToken&& token
        NETIO_DEFAULT_COMPLETION_TOKEN(Executor))
    -> decltype(
        async_initiate<CompletionToken, void (netio::error_code)>(
          declval<typename conditional_t<false, CompletionToken,
            Derived>::initiate_async_send>(), token,
          declval<typename conditional_t<false, CompletionToken,
            Derived>::payload_type>()))
  {
    typedef typename Derived::payload_type payload_type;
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return async_initiate<CompletionToken, void (netio::error_code)>(
        typename Derived::initiate_async_send(self), token,
        payload_type(message_type(0, static_cast<Args&&>(args)...)));
  }
};

template <typename Derived, typename Executor,
    typename R, typename... Args, typename... Signatures>
class channel_send_functions<Derived, Executor, R(Args...), Signatures...> :
  public channel_send_functions<Derived, Executor, Signatures...>
{
public:
  using channel_send_functions<Derived, Executor, Signatures...>::try_send;
  using channel_send_functions<Derived, Executor, Signatures...>::async_send;

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    bool
  > try_send(Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send<message_type>(
        self->impl_, false, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    bool
  > try_send_via_dispatch(Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send<message_type>(
        self->impl_, true, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    std::size_t
  > try_send_n(std::size_t count, Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send_n<message_type>(
        self->impl_, count, false, static_cast<Args2&&>(args)...);
  }

  template <typename... Args2>
  enable_if_t<
    is_constructible<netio::detail::completion_message<R(Args...)>,
      int, Args2...>::value,
    std::size_t
  > try_send_n_via_dispatch(std::size_t count, Args2&&... args)
  {
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return self->service_->template try_send_n<message_type>(
        self->impl_, count, true, static_cast<Args2&&>(args)...);
  }

  template <
      NETIO_COMPLETION_TOKEN_FOR(void (netio::error_code))
        CompletionToken NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
  auto async_send(Args... args,
      CompletionToken&& token
        NETIO_DEFAULT_COMPLETION_TOKEN(Executor))
    -> decltype(
        async_initiate<CompletionToken, void (netio::error_code)>(
          declval<typename conditional_t<false, CompletionToken,
            Derived>::initiate_async_send>(), token,
          declval<typename conditional_t<false, CompletionToken,
            Derived>::payload_type>()))
  {
    typedef typename Derived::payload_type payload_type;
    typedef netio::detail::completion_message<R(Args...)> message_type;
    Derived* self = static_cast<Derived*>(this);
    return async_initiate<CompletionToken, void (netio::error_code)>(
        typename Derived::initiate_async_send(self), token,
        payload_type(message_type(0, static_cast<Args&&>(args)...)));
  }
};

} // namespace detail
} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_DETAIL_CHANNEL_SEND_FUNCTIONS_HPP
