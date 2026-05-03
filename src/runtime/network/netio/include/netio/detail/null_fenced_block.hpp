#ifndef NETIO_DETAIL_NULL_FENCED_BLOCK_HPP
#define NETIO_DETAIL_NULL_FENCED_BLOCK_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/noncopyable.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class null_fenced_block
  : private noncopyable
{
public:
  enum half_or_full_t { half, full };

  // Constructor.
  explicit null_fenced_block(half_or_full_t)
  {
  }

  // Destructor.
  ~null_fenced_block()
  {
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_NULL_FENCED_BLOCK_HPP
