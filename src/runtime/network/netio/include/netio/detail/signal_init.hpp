#ifndef NETIO_DETAIL_SIGNAL_INIT_HPP
#define NETIO_DETAIL_SIGNAL_INIT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS) && !defined(__CYGWIN__)

#include <csignal>

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <int Signal = SIGPIPE>
class signal_init
{
public:
  // Constructor.
  signal_init()
  {
    std::signal(Signal, SIG_IGN);
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_WINDOWS) && !defined(__CYGWIN__)

#endif // NETIO_DETAIL_SIGNAL_INIT_HPP
