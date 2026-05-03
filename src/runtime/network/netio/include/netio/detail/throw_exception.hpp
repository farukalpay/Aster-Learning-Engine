#ifndef NETIO_DETAIL_THROW_EXCEPTION_HPP
#define NETIO_DETAIL_THROW_EXCEPTION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_BOOST_THROW_EXCEPTION)
# include <boost/throw_exception.hpp>
#endif // defined(NETIO_BOOST_THROW_EXCEPTION)

namespace netio {
namespace detail {

#if defined(NETIO_HAS_BOOST_THROW_EXCEPTION)
using boost::throw_exception;
#else // defined(NETIO_HAS_BOOST_THROW_EXCEPTION)

// Declare the throw_exception function for all targets.
template <typename Exception>
[[noreturn]] void throw_exception(
    const Exception& e
    NETIO_SOURCE_LOCATION_DEFAULTED_PARAM);

// Only define the throw_exception function when exceptions are enabled.
// Otherwise, it is up to the application to provide a definition of this
// function.
# if !defined(NETIO_NO_EXCEPTIONS)
template <typename Exception>
void throw_exception(
    const Exception& e
    NETIO_SOURCE_LOCATION_PARAM)
{
  throw e;
}
# endif // !defined(NETIO_NO_EXCEPTIONS)

#endif // defined(NETIO_HAS_BOOST_THROW_EXCEPTION)

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_THROW_EXCEPTION_HPP
