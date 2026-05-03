#ifndef NETIO_DETAIL_RESOLVE_OP_HPP
#define NETIO_DETAIL_RESOLVE_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/error.hpp"
#include "netio/detail/operation.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class resolve_op : public operation
{
public:
  // The error code to be passed to the completion handler.
  netio::error_code ec_;

protected:
  resolve_op(func_type complete_func)
    : operation(complete_func)
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_RESOLVE_OP_HPP
