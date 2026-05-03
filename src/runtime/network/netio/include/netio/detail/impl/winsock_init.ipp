#ifndef NETIO_DETAIL_IMPL_WINSOCK_INIT_IPP
#define NETIO_DETAIL_IMPL_WINSOCK_INIT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS) || defined(__CYGWIN__)

#include "netio/detail/socket_types.hpp"
#include "netio/detail/winsock_init.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

void winsock_init_base::startup(data& d,
    unsigned char major, unsigned char minor)
{
  if (::InterlockedIncrement(&d.init_count_) == 1)
  {
    WSADATA wsa_data;
    long result = ::WSAStartup(MAKEWORD(major, minor), &wsa_data);
    ::InterlockedExchange(&d.result_, result);
  }
}

void winsock_init_base::manual_startup(data& d)
{
  if (::InterlockedIncrement(&d.init_count_) == 1)
  {
    ::InterlockedExchange(&d.result_, 0);
  }
}

void winsock_init_base::cleanup(data& d)
{
  if (::InterlockedDecrement(&d.init_count_) == 0)
  {
    ::WSACleanup();
  }
}

void winsock_init_base::manual_cleanup(data& d)
{
  ::InterlockedDecrement(&d.init_count_);
}

void winsock_init_base::throw_on_error(data& d)
{
  long result = ::InterlockedExchangeAdd(&d.result_, 0);
  if (result != 0)
  {
    netio::error_code ec(result,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "winsock");
  }
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS) || defined(__CYGWIN__)

#endif // NETIO_DETAIL_IMPL_WINSOCK_INIT_IPP
