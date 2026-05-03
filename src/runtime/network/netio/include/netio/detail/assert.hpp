#ifndef NETIO_DETAIL_ASSERT_HPP
#define NETIO_DETAIL_ASSERT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_BOOST_ASSERT)
# include <boost/assert.hpp>
#else // defined(NETIO_HAS_BOOST_ASSERT)
# include <cassert>
#endif // defined(NETIO_HAS_BOOST_ASSERT)

#if defined(NETIO_HAS_BOOST_ASSERT)
# define NETIO_ASSERT(expr) BOOST_ASSERT(expr)
#else // defined(NETIO_HAS_BOOST_ASSERT)
# define NETIO_ASSERT(expr) assert(expr)
#endif // defined(NETIO_HAS_BOOST_ASSERT)

#endif // NETIO_DETAIL_ASSERT_HPP
