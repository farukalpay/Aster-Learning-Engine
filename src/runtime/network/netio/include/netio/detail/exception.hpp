#ifndef NETIO_DETAIL_EXCEPTION_HPP
#define NETIO_DETAIL_EXCEPTION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <exception>

namespace netio {

using std::exception_ptr;
using std::current_exception;
using std::rethrow_exception;

} // namespace netio

#endif // NETIO_DETAIL_EXCEPTION_HPP
