#ifndef NETIO_DETAIL_SCHEDULER_THREAD_INFO_HPP
#define NETIO_DETAIL_SCHEDULER_THREAD_INFO_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/op_queue.hpp"
#include "netio/detail/thread_info_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class scheduler;
class scheduler_operation;

struct scheduler_thread_info : public thread_info_base
{
  op_queue<scheduler_operation> private_op_queue;
  long private_outstanding_work;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_SCHEDULER_THREAD_INFO_HPP
