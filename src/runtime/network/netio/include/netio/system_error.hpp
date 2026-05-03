#ifndef NETIO_SYSTEM_ERROR_HPP
#define NETIO_SYSTEM_ERROR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <system_error>

#include "netio/detail/push_options.hpp"

namespace netio {

typedef std::system_error system_error;

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SYSTEM_ERROR_HPP
