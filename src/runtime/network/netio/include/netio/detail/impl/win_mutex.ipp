#ifndef NETIO_DETAIL_IMPL_WIN_MUTEX_IPP
#define NETIO_DETAIL_IMPL_WIN_MUTEX_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include "netio/detail/throw_error.hpp"
#include "netio/detail/win_mutex.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

win_mutex::win_mutex()
{
  int error = do_init();
  netio::error_code ec(error,
      netio::error::get_system_category());
  netio::detail::throw_error(ec, "mutex");
}

int win_mutex::do_init()
{
#if defined(__MINGW32__)
  // Not sure if MinGW supports structured exception handling, so for now
  // we'll just call the Windows API and hope.
# if defined(UNDER_CE)
  ::InitializeCriticalSection(&crit_section_);
# elif defined(NETIO_WINDOWS_APP)
  if (!::InitializeCriticalSectionEx(&crit_section_, 0, 0))
    return ::GetLastError();
# else
  if (!::InitializeCriticalSectionAndSpinCount(&crit_section_, 0x80000000))
    return ::GetLastError();
# endif
  return 0;
#else
  __try
  {
# if defined(UNDER_CE)
    ::InitializeCriticalSection(&crit_section_);
# elif defined(NETIO_WINDOWS_APP)
    if (!::InitializeCriticalSectionEx(&crit_section_, 0, 0))
      return ::GetLastError();
# else
    if (!::InitializeCriticalSectionAndSpinCount(&crit_section_, 0x80000000))
      return ::GetLastError();
# endif
  }
  __except(GetExceptionCode() == STATUS_NO_MEMORY
      ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
  {
    return ERROR_OUTOFMEMORY;
  }

  return 0;
#endif
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_IMPL_WIN_MUTEX_IPP
