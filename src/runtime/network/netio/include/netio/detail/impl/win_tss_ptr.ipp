#ifndef NETIO_DETAIL_IMPL_WIN_TSS_PTR_IPP
#define NETIO_DETAIL_IMPL_WIN_TSS_PTR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include "netio/detail/throw_error.hpp"
#include "netio/detail/win_tss_ptr.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

DWORD win_tss_ptr_create()
{
#if defined(UNDER_CE)
  const DWORD out_of_indexes = 0xFFFFFFFF;
#else
  const DWORD out_of_indexes = TLS_OUT_OF_INDEXES;
#endif

  DWORD tss_key = ::TlsAlloc();
  if (tss_key == out_of_indexes)
  {
    DWORD last_error = ::GetLastError();
    netio::error_code ec(last_error,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "tss");
  }
  return tss_key;
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_IMPL_WIN_TSS_PTR_IPP
