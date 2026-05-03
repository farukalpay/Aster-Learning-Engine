#ifndef NETIO_TIME_TRAITS_HPP
#define NETIO_TIME_TRAITS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/socket_types.hpp" // Must come before posix_time.

#if !defined(NETIO_NO_DEPRECATED)

#if defined(NETIO_HAS_BOOST_DATE_TIME) \
  || defined(GENERATING_DOCUMENTATION)

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "netio/detail/push_options.hpp"

namespace netio {

/// (Deprecated) Time traits suitable for use with the deadline timer.
template <typename Time>
struct NETIO_DEPRECATED_MSG("Use basic_waitable_timer and wait_traits")
  time_traits;

/// (Deprecated) Time traits specialised for posix_time.
template <>
struct NETIO_DEPRECATED_MSG("Use basic_waitable_timer and wait_traits")
  time_traits<boost::posix_time::ptime>
{
  /// The time type.
  typedef boost::posix_time::ptime time_type;

  /// The duration type.
  typedef boost::posix_time::time_duration duration_type;

  /// Get the current time.
  static time_type now()
  {
#if defined(BOOST_DATE_TIME_HAS_HIGH_PRECISION_CLOCK)
    return boost::posix_time::microsec_clock::universal_time();
#else // defined(BOOST_DATE_TIME_HAS_HIGH_PRECISION_CLOCK)
    return boost::posix_time::second_clock::universal_time();
#endif // defined(BOOST_DATE_TIME_HAS_HIGH_PRECISION_CLOCK)
  }

  /// Add a duration to a time.
  static time_type add(const time_type& t, const duration_type& d)
  {
    return t + d;
  }

  /// Subtract one time from another.
  static duration_type subtract(const time_type& t1, const time_type& t2)
  {
    return t1 - t2;
  }

  /// Test whether one time is less than another.
  static bool less_than(const time_type& t1, const time_type& t2)
  {
    return t1 < t2;
  }

  /// Convert to POSIX duration type.
  static boost::posix_time::time_duration to_posix_duration(
      const duration_type& d)
  {
    return d;
  }
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_BOOST_DATE_TIME)
       // || defined(GENERATING_DOCUMENTATION)

#endif // !defined(NETIO_NO_DEPRECATED)

#endif // NETIO_TIME_TRAITS_HPP
