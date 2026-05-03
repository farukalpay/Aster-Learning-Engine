#ifndef NETIO_DETAIL_NULL_REACTOR_HPP
#define NETIO_DETAIL_NULL_REACTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP) \
  || defined(NETIO_WINDOWS_RUNTIME) \
  || defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#include "netio/detail/scheduler_operation.hpp"
#include "netio/detail/scheduler_task.hpp"
#include "netio/execution_context.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class null_reactor
  : public execution_context_service_base<null_reactor>,
    public scheduler_task
{
public:
  struct per_descriptor_data
  {
  };

  // Constructor.
  null_reactor(netio::execution_context& ctx)
    : execution_context_service_base<null_reactor>(ctx)
  {
  }

  // Destructor.
  ~null_reactor()
  {
  }

  // Initialise the task.
  void init_task()
  {
  }

  // Destroy all user-defined handler objects owned by the service.
  void shutdown()
  {
  }

  // No-op because should never be called.
  void run(long /*usec*/, op_queue<scheduler_operation>& /*ops*/)
  {
  }

  // No-op.
  void interrupt()
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IOCP)
       //   || defined(NETIO_WINDOWS_RUNTIME)
       //   || defined(NETIO_HAS_IO_URING_AS_DEFAULT)

#endif // NETIO_DETAIL_NULL_REACTOR_HPP
