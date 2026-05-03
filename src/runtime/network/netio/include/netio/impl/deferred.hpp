#ifndef NETIO_IMPL_DEFERRED_HPP
#define NETIO_IMPL_DEFERRED_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/push_options.hpp"

namespace netio {

#if !defined(GENERATING_DOCUMENTATION)

template <typename Signature>
class async_result<deferred_t, Signature>
{
public:
  template <typename Initiation, typename... InitArgs>
  static deferred_async_operation<Signature, Initiation, InitArgs...>
  initiate(Initiation&& initiation, deferred_t, InitArgs&&... args)
  {
    return deferred_async_operation<Signature, Initiation, InitArgs...>(
        deferred_init_tag{},
        static_cast<Initiation&&>(initiation),
        static_cast<InitArgs&&>(args)...);
  }
};

template <typename... Signatures>
class async_result<deferred_t, Signatures...>
{
public:
  template <typename Initiation, typename... InitArgs>
  static deferred_async_operation<
      deferred_signatures<Signatures...>, Initiation, InitArgs...>
  initiate(Initiation&& initiation, deferred_t, InitArgs&&... args)
  {
    return deferred_async_operation<
        deferred_signatures<Signatures...>, Initiation, InitArgs...>(
          deferred_init_tag{},
          static_cast<Initiation&&>(initiation),
          static_cast<InitArgs&&>(args)...);
  }
};

template <typename Function, typename Signature>
class async_result<deferred_function<Function>, Signature>
{
public:
  template <typename Initiation, typename... InitArgs>
  static auto initiate(Initiation&& initiation,
      deferred_function<Function> token, InitArgs&&... init_args)
    -> decltype(
        deferred_sequence<
          deferred_async_operation<
            Signature, Initiation, InitArgs...>,
          Function>(deferred_init_tag{},
            deferred_async_operation<
              Signature, Initiation, InitArgs...>(
                deferred_init_tag{},
                static_cast<Initiation&&>(initiation),
                static_cast<InitArgs&&>(init_args)...),
            static_cast<Function&&>(token.function_)))
  {
    return deferred_sequence<
        deferred_async_operation<
          Signature, Initiation, InitArgs...>,
        Function>(deferred_init_tag{},
          deferred_async_operation<
            Signature, Initiation, InitArgs...>(
              deferred_init_tag{},
              static_cast<Initiation&&>(initiation),
              static_cast<InitArgs&&>(init_args)...),
          static_cast<Function&&>(token.function_));
  }
};

template <typename Function, typename... Signatures>
class async_result<deferred_function<Function>, Signatures...>
{
public:
  template <typename Initiation, typename... InitArgs>
  static auto initiate(Initiation&& initiation,
      deferred_function<Function> token, InitArgs&&... init_args)
    -> decltype(
        deferred_sequence<
          deferred_async_operation<
            deferred_signatures<Signatures...>, Initiation, InitArgs...>,
          Function>(deferred_init_tag{},
            deferred_async_operation<
              deferred_signatures<Signatures...>, Initiation, InitArgs...>(
                deferred_init_tag{},
                static_cast<Initiation&&>(initiation),
                static_cast<InitArgs&&>(init_args)...),
            static_cast<Function&&>(token.function_)))
  {
    return deferred_sequence<
        deferred_async_operation<
          deferred_signatures<Signatures...>, Initiation, InitArgs...>,
        Function>(deferred_init_tag{},
          deferred_async_operation<
            deferred_signatures<Signatures...>, Initiation, InitArgs...>(
              deferred_init_tag{},
              static_cast<Initiation&&>(initiation),
              static_cast<InitArgs&&>(init_args)...),
          static_cast<Function&&>(token.function_));
  }
};

template <template <typename, typename> class Associator,
    typename Handler, typename Tail, typename DefaultCandidate>
struct associator<Associator,
    detail::deferred_sequence_handler<Handler, Tail>,
    DefaultCandidate>
  : Associator<Handler, DefaultCandidate>
{
  static typename Associator<Handler, DefaultCandidate>::type get(
      const detail::deferred_sequence_handler<Handler, Tail>& h) noexcept
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_);
  }

  static auto get(const detail::deferred_sequence_handler<Handler, Tail>& h,
      const DefaultCandidate& c) noexcept
    -> decltype(Associator<Handler, DefaultCandidate>::get(h.handler_, c))
  {
    return Associator<Handler, DefaultCandidate>::get(h.handler_, c);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_DEFERRED_HPP
