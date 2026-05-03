#ifndef NETIO_DETAIL_REACTOR_OP_HPP
#define NETIO_DETAIL_REACTOR_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class reactor_op
  : public operation
{
public:
  // The error code to be passed to the completion handler.
  netio::error_code ec_;

  // The operation key used for targeted cancellation.
  void* cancellation_key_;

  // The number of bytes transferred, to be passed to the completion handler.
  std::size_t bytes_transferred_;

  // Status returned by perform function. May be used to decide whether it is
  // worth performing more operations on the descriptor immediately.
  enum status { not_done, done, done_and_exhausted };

  // Perform the operation. Returns true if it is finished.
  status perform()
  {
    return perform_func_(this);
  }

protected:
  typedef status (*perform_func_type)(reactor_op*);

  reactor_op(const netio::error_code& success_ec,
      perform_func_type perform_func, func_type complete_func)
    : operation(complete_func),
      ec_(success_ec),
      cancellation_key_(0),
      bytes_transferred_(0),
      perform_func_(perform_func)
  {
  }

private:
  perform_func_type perform_func_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_REACTOR_OP_HPP
