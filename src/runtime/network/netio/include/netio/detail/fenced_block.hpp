#ifndef NETIO_DETAIL_FENCED_BLOCK_HPP
#define NETIO_DETAIL_FENCED_BLOCK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS) \
  || defined(NETIO_DISABLE_FENCED_BLOCK)
# include "netio/detail/null_fenced_block.hpp"
#else
# include "netio/detail/std_fenced_block.hpp"
#endif

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS) \
  || defined(NETIO_DISABLE_FENCED_BLOCK)
typedef null_fenced_block fenced_block;
#else
typedef std_fenced_block fenced_block;
#endif

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_FENCED_BLOCK_HPP
