#ifndef NETIO_DETAIL_IMPL_THROW_ERROR_IPP
#define NETIO_DETAIL_IMPL_THROW_ERROR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/system_error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

void do_throw_error(
    const netio::error_code& err
    NETIO_SOURCE_LOCATION_PARAM)
{
  netio::system_error e(err);
  netio::detail::throw_exception(e NETIO_SOURCE_LOCATION_ARG);
}

void do_throw_error(
    const netio::error_code& err,
    const char* location
    NETIO_SOURCE_LOCATION_PARAM)
{
  netio::system_error e(err, location);
  netio::detail::throw_exception(e NETIO_SOURCE_LOCATION_ARG);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_IMPL_THROW_ERROR_IPP
