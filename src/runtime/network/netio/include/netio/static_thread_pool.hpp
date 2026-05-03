#ifndef NETIO_STATIC_THREAD_POOL_HPP
#define NETIO_STATIC_THREAD_POOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/thread_pool.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

typedef thread_pool static_thread_pool;

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_STATIC_THREAD_POOL_HPP
