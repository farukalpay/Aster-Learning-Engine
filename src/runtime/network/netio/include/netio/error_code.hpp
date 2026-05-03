#ifndef NETIO_ERROR_CODE_HPP
#define NETIO_ERROR_CODE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <system_error>

#include "netio/detail/push_options.hpp"

namespace netio {

typedef std::error_category error_category;
typedef std::error_code error_code;

/// Returns the error category used for the system errors produced by netio.
extern NETIO_DECL const error_category& system_category();

} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/impl/error_code.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_ERROR_CODE_HPP
