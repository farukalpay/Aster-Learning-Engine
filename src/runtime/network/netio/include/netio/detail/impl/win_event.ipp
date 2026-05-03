#ifndef NETIO_DETAIL_IMPL_WIN_EVENT_IPP
#define NETIO_DETAIL_IMPL_WIN_EVENT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS)

#include "netio/detail/throw_error.hpp"
#include "netio/detail/win_event.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

win_event::win_event()
  : state_(0)
{
#if defined(NETIO_WINDOWS_APP)
  events_[0] = ::CreateEventExW(0, 0,
      CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
#else // defined(NETIO_WINDOWS_APP)
  events_[0] = ::CreateEventW(0, true, false, 0);
#endif // defined(NETIO_WINDOWS_APP)
  if (!events_[0])
  {
    DWORD last_error = ::GetLastError();
    netio::error_code ec(last_error,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "event");
  }

#if defined(NETIO_WINDOWS_APP)
  events_[1] = ::CreateEventExW(0, 0, 0, EVENT_ALL_ACCESS);
#else // defined(NETIO_WINDOWS_APP)
  events_[1] = ::CreateEventW(0, false, false, 0);
#endif // defined(NETIO_WINDOWS_APP)
  if (!events_[1])
  {
    DWORD last_error = ::GetLastError();
    ::CloseHandle(events_[0]);
    netio::error_code ec(last_error,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "event");
  }
}

win_event::~win_event()
{
  ::CloseHandle(events_[0]);
  ::CloseHandle(events_[1]);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS)

#endif // NETIO_DETAIL_IMPL_WIN_EVENT_IPP
