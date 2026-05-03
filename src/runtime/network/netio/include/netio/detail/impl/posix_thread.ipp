#ifndef NETIO_DETAIL_IMPL_POSIX_THREAD_IPP
#define NETIO_DETAIL_IMPL_POSIX_THREAD_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include "netio/detail/posix_thread.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

posix_thread::~posix_thread()
{
  if (arg_)
    std::terminate();
}

void posix_thread::join()
{
  if (arg_)
  {
    ::pthread_join(arg_->thread_, 0);
    arg_->destroy();
    arg_ = 0;
  }
}

std::size_t posix_thread::hardware_concurrency()
{
#if defined(_SC_NPROCESSORS_ONLN)
  long result = sysconf(_SC_NPROCESSORS_ONLN);
  if (result > 0)
    return result;
#endif // defined(_SC_NPROCESSORS_ONLN)
  return 0;
}

posix_thread::func_base* posix_thread::start_thread(func_base* arg)
{
  int error = ::pthread_create(&arg->thread_, 0,
        netio_detail_posix_thread_function, arg);
  if (error != 0)
  {
    arg->destroy();
    netio::error_code ec(error,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "thread");
  }
  return arg;
}

void* netio_detail_posix_thread_function(void* arg)
{
  static_cast<posix_thread::func_base*>(arg)->run();
  return 0;
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_IMPL_POSIX_THREAD_IPP
