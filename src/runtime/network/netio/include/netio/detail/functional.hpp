#ifndef NETIO_DETAIL_FUNCTIONAL_HPP
#define NETIO_DETAIL_FUNCTIONAL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <functional>

namespace netio {
namespace detail {

using std::function;

} // namespace detail

using std::ref;
using std::reference_wrapper;

} // namespace netio

#endif // NETIO_DETAIL_FUNCTIONAL_HPP
