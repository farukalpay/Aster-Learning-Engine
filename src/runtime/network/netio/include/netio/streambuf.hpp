#ifndef NETIO_STREAMBUF_HPP
#define NETIO_STREAMBUF_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_NO_IOSTREAM)

#include "netio/basic_streambuf.hpp"

namespace netio {

/// Typedef for the typical usage of basic_streambuf.
typedef basic_streambuf<> streambuf;

} // namespace netio

#endif // !defined(NETIO_NO_IOSTREAM)

#endif // NETIO_STREAMBUF_HPP
