#ifndef NETIO_SIGNAL_SET_HPP
#define NETIO_SIGNAL_SET_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/basic_signal_set.hpp"

namespace netio {

/// Typedef for the typical usage of a signal set.
typedef basic_signal_set<> signal_set;

} // namespace netio

#endif // NETIO_SIGNAL_SET_HPP
