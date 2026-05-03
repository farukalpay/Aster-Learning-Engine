#ifndef NETIO_DETAIL_DATE_TIME_FWD_HPP
#define NETIO_DETAIL_DATE_TIME_FWD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

namespace boost {
namespace date_time {

template<class T, class TimeSystem>
class base_time;

} // namespace date_time
namespace posix_time {

class ptime;

} // namespace posix_time
} // namespace boost

#endif // NETIO_DETAIL_DATE_TIME_FWD_HPP
