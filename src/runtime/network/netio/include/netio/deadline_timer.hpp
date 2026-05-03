#ifndef NETIO_DEADLINE_TIMER_HPP
#define NETIO_DEADLINE_TIMER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_NO_DEPRECATED)

#if defined(NETIO_HAS_BOOST_DATE_TIME) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/detail/socket_types.hpp" // Must come before posix_time.
#include "netio/basic_deadline_timer.hpp"

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace netio {

/// (Deprecated: Use system_timer.) Typedef for the typical usage of timer. Uses
/// a UTC clock.
NETIO_DEPRECATED_MSG("Use system_timer")
typedef basic_deadline_timer<boost::posix_time::ptime> deadline_timer;

} // namespace netio

#endif // defined(NETIO_HAS_BOOST_DATE_TIME)
       // || defined(GENERATING_DOCUMENTATION)

#endif // !defined(NETIO_NO_DEPRECATED)

#endif // NETIO_DEADLINE_TIMER_HPP
