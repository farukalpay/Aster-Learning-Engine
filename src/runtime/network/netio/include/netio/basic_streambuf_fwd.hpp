#ifndef NETIO_BASIC_STREAMBUF_FWD_HPP
#define NETIO_BASIC_STREAMBUF_FWD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_NO_IOSTREAM)

#include <memory>

namespace netio {

template <typename Allocator = std::allocator<char>>
class basic_streambuf;

template <typename Allocator = std::allocator<char>>
class basic_streambuf_ref;

} // namespace netio

#endif // !defined(NETIO_NO_IOSTREAM)

#endif // NETIO_BASIC_STREAMBUF_FWD_HPP
