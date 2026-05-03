#ifndef NETIO_DETAIL_FUTURE_HPP
#define NETIO_DETAIL_FUTURE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <future>

// Even though the future header is available, libstdc++ may not implement the
// std::future class itself. However, we need to have already included the
// future header to reliably test for _GLIBCXX_HAS_GTHREADS.
#if defined(__GNUC__) && !defined(NETIO_HAS_CLANG_LIBCXX)
# if defined(_GLIBCXX_HAS_GTHREADS)
#  define NETIO_HAS_STD_FUTURE_CLASS 1
# endif // defined(_GLIBCXX_HAS_GTHREADS)
#else // defined(__GNUC__) && !defined(NETIO_HAS_CLANG_LIBCXX)
# define NETIO_HAS_STD_FUTURE_CLASS 1
#endif // defined(__GNUC__) && !defined(NETIO_HAS_CLANG_LIBCXX)

#endif // NETIO_DETAIL_FUTURE_HPP
