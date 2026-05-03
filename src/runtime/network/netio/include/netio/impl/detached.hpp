#ifndef NETIO_IMPL_DETACHED_HPP
#define NETIO_IMPL_DETACHED_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/async_result.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

  // Class to adapt a detached_t as a completion handler.
  class detached_handler
  {
  public:
    typedef void result_type;

    detached_handler(detached_t)
    {
    }

    template <typename... Args>
    void operator()(Args...)
    {
    }
  };

} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename... Signatures>
struct async_result<detached_t, Signatures...>
{
  typedef netio::detail::detached_handler completion_handler_type;

  typedef void return_type;

  explicit async_result(completion_handler_type&)
  {
  }

  void get()
  {
  }

  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static return_type initiate(Initiation&& initiation,
      RawCompletionToken&&, Args&&... args)
  {
    static_cast<Initiation&&>(initiation)(
        detail::detached_handler(detached_t()),
        static_cast<Args&&>(args)...);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_DETACHED_HPP
