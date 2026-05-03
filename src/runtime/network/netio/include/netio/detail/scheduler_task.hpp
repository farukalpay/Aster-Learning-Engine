#ifndef NETIO_DETAIL_SCHEDULER_TASK_HPP
#define NETIO_DETAIL_SCHEDULER_TASK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/op_queue.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class scheduler_operation;

// Base class for all tasks that may be run by a scheduler.
class scheduler_task
{
public:
  // Run the task once until interrupted or events are ready to be dispatched.
  virtual void run(long usec, op_queue<scheduler_operation>& ops) = 0;

  // Interrupt the task.
  virtual void interrupt() = 0;

protected:
  // Prevent deletion through this type.
  ~scheduler_task()
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_SCHEDULER_TASK_HPP
