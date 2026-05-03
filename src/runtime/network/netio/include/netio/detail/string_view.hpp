#ifndef NETIO_DETAIL_STRING_VIEW_HPP
#define NETIO_DETAIL_STRING_VIEW_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_STRING_VIEW)

#if defined(NETIO_HAS_STD_STRING_VIEW)
# include <string_view>
#elif defined(NETIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
# include <experimental/string_view>
#else // defined(NETIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
# error NETIO_HAS_STRING_VIEW is set but no string_view is available
#endif // defined(NETIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)

namespace netio {

#if defined(NETIO_HAS_STD_STRING_VIEW)
using std::basic_string_view;
using std::string_view;
#elif defined(NETIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
using std::experimental::basic_string_view;
using std::experimental::string_view;
#endif // defined(NETIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)

} // namespace netio

# define NETIO_STRING_VIEW_PARAM netio::string_view
#else // defined(NETIO_HAS_STRING_VIEW)
# define NETIO_STRING_VIEW_PARAM const std::string&
#endif // defined(NETIO_HAS_STRING_VIEW)

#endif // NETIO_DETAIL_STRING_VIEW_HPP
