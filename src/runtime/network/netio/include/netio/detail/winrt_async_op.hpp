#ifndef NETIO_DETAIL_WINRT_ASYNC_OP_HPP
#define NETIO_DETAIL_WINRT_ASYNC_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename TResult>
class winrt_async_op
  : public operation
{
public:
  // The error code to be passed to the completion handler.
  netio::error_code ec_;

  // The result of the operation, to be passed to the completion handler.
  TResult result_;

protected:
  winrt_async_op(func_type complete_func)
    : operation(complete_func),
      result_()
  {
  }
};

template <>
class winrt_async_op<void>
  : public operation
{
public:
  // The error code to be passed to the completion handler.
  netio::error_code ec_;

protected:
  winrt_async_op(func_type complete_func)
    : operation(complete_func)
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_WINRT_ASYNC_OP_HPP
