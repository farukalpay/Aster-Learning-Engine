#ifndef NETIO_DETAIL_SOURCE_LOCATION_HPP
#define NETIO_DETAIL_SOURCE_LOCATION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_SOURCE_LOCATION)

#if defined(NETIO_HAS_STD_SOURCE_LOCATION)
# include <source_location>
#elif defined(NETIO_HAS_STD_EXPERIMENTAL_SOURCE_LOCATION)
# include <experimental/source_location>
#else // defined(NETIO_HAS_STD_EXPERIMENTAL_SOURCE_LOCATION)
# error NETIO_HAS_SOURCE_LOCATION is set \
  but no source_location is available
#endif // defined(NETIO_HAS_STD_EXPERIMENTAL_SOURCE_LOCATION)

namespace netio {
namespace detail {

#if defined(NETIO_HAS_STD_SOURCE_LOCATION)
using std::source_location;
#elif defined(NETIO_HAS_STD_EXPERIMENTAL_SOURCE_LOCATION)
using std::experimental::source_location;
#endif // defined(NETIO_HAS_STD_EXPERIMENTAL_SOURCE_LOCATION)

} // namespace detail
} // namespace netio

#endif // defined(NETIO_HAS_SOURCE_LOCATION)

#endif // NETIO_DETAIL_SOURCE_LOCATION_HPP
