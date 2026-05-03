#ifndef NETIO_IMPL_EXECUTOR_IPP
#define NETIO_IMPL_EXECUTOR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_NO_TS_EXECUTORS)

#include "netio/executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

bad_executor::bad_executor() noexcept
{
}

const char* bad_executor::what() const noexcept
{
  return "bad executor";
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_NO_TS_EXECUTORS)

#endif // NETIO_IMPL_EXECUTOR_IPP
