#ifndef NETIO_STEADY_TIMER_HPP
#define NETIO_STEADY_TIMER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/basic_waitable_timer.hpp"
#include "netio/detail/chrono.hpp"

namespace netio {

/// Typedef for a timer based on the steady clock.
/**
 * This typedef uses the C++11 @c &lt;chrono&gt; standard library facility, if
 * available. Otherwise, it may use the Boost.Chrono library. To explicitly
 * utilise Boost.Chrono, use the basic_waitable_timer template directly:
 * @code
 * typedef basic_waitable_timer<boost::chrono::steady_clock> timer;
 * @endcode
 */
typedef basic_waitable_timer<chrono::steady_clock> steady_timer;

} // namespace netio

#endif // NETIO_STEADY_TIMER_HPP
