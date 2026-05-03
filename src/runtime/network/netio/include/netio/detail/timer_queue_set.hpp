#ifndef NETIO_DETAIL_TIMER_QUEUE_SET_HPP
#define NETIO_DETAIL_TIMER_QUEUE_SET_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/timer_queue_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class timer_queue_set
{
public:
  // Constructor.
  NETIO_DECL timer_queue_set();

  // Add a timer queue to the set.
  NETIO_DECL void insert(timer_queue_base* q);

  // Remove a timer queue from the set.
  NETIO_DECL void erase(timer_queue_base* q);

  // Determine whether all queues are empty.
  NETIO_DECL bool all_empty() const;

  // Get the wait duration in milliseconds.
  NETIO_DECL long wait_duration_msec(long max_duration) const;

  // Get the wait duration in microseconds.
  NETIO_DECL long wait_duration_usec(long max_duration) const;

  // Dequeue all ready timers.
  NETIO_DECL void get_ready_timers(op_queue<operation>& ops);

  // Dequeue all timers.
  NETIO_DECL void get_all_timers(op_queue<operation>& ops);

private:
  timer_queue_base* first_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/timer_queue_set.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_TIMER_QUEUE_SET_HPP
