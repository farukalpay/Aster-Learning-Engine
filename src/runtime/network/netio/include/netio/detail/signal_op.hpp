#ifndef NETIO_DETAIL_SIGNAL_OP_HPP
#define NETIO_DETAIL_SIGNAL_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class signal_op
  : public operation
{
public:
  // The error code to be passed to the completion handler.
  netio::error_code ec_;

  // The operation key used for targeted cancellation.
  void* cancellation_key_;

  // The signal number to be passed to the completion handler.
  int signal_number_;

protected:
  signal_op(func_type func)
    : operation(func),
      cancellation_key_(0),
      signal_number_(0)
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_SIGNAL_OP_HPP
