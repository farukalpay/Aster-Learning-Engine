#ifndef NETIO_PACKAGED_TASK_HPP
#define NETIO_PACKAGED_TASK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/future.hpp"

#if defined(NETIO_HAS_STD_FUTURE_CLASS) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/async_result.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// Partial specialisation of @c async_result for @c std::packaged_task.
template <typename Result, typename... Args, typename Signature>
class async_result<std::packaged_task<Result(Args...)>, Signature>
{
public:
  /// The packaged task is the concrete completion handler type.
  typedef std::packaged_task<Result(Args...)> completion_handler_type;

  /// The return type of the initiating function is the future obtained from
  /// the packaged task.
  typedef std::future<Result> return_type;

  /// The constructor extracts the future from the packaged task.
  explicit async_result(completion_handler_type& h)
    : future_(h.get_future())
  {
  }

  /// Returns the packaged task's future.
  return_type get()
  {
    return std::move(future_);
  }

private:
  return_type future_;
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_STD_FUTURE_CLASS)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_PACKAGED_TASK_HPP
