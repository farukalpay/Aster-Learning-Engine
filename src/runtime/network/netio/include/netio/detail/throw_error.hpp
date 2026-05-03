#ifndef NETIO_DETAIL_THROW_ERROR_HPP
#define NETIO_DETAIL_THROW_ERROR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/throw_exception.hpp"
#include "netio/error_code.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

NETIO_DECL void do_throw_error(
    const netio::error_code& err
    NETIO_SOURCE_LOCATION_PARAM);

NETIO_DECL void do_throw_error(
    const netio::error_code& err,
    const char* location
    NETIO_SOURCE_LOCATION_PARAM);

inline void throw_error(
    const netio::error_code& err
    NETIO_SOURCE_LOCATION_DEFAULTED_PARAM)
{
  if (err)
    do_throw_error(err NETIO_SOURCE_LOCATION_ARG);
}

inline void throw_error(
    const netio::error_code& err,
    const char* location
    NETIO_SOURCE_LOCATION_DEFAULTED_PARAM)
{
  if (err)
    do_throw_error(err, location NETIO_SOURCE_LOCATION_ARG);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/throw_error.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_THROW_ERROR_HPP
