#ifndef NETIO_DETAIL_LOCAL_FREE_ON_BLOCK_EXIT_HPP
#define NETIO_DETAIL_LOCAL_FREE_ON_BLOCK_EXIT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS) || defined(__CYGWIN__)
#if !defined(NETIO_WINDOWS_APP)

#include "netio/detail/noncopyable.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class local_free_on_block_exit
  : private noncopyable
{
public:
  // Constructor blocks all signals for the calling thread.
  explicit local_free_on_block_exit(void* p)
    : p_(p)
  {
  }

  // Destructor restores the previous signal mask.
  ~local_free_on_block_exit()
  {
    ::LocalFree(p_);
  }

private:
  void* p_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_WINDOWS_APP)
#endif // defined(NETIO_WINDOWS) || defined(__CYGWIN__)

#endif // NETIO_DETAIL_LOCAL_FREE_ON_BLOCK_EXIT_HPP
