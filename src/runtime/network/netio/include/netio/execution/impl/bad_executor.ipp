#ifndef NETIO_EXECUTION_IMPL_BAD_EXECUTOR_IPP
#define NETIO_EXECUTION_IMPL_BAD_EXECUTOR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/execution/bad_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace execution {

bad_executor::bad_executor() noexcept
{
}

const char* bad_executor::what() const noexcept
{
  return "bad executor";
}

} // namespace execution
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXECUTION_IMPL_BAD_EXECUTOR_IPP
