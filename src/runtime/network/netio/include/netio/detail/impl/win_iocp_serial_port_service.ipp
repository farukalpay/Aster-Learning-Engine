#ifndef NETIO_DETAIL_IMPL_WIN_IOCP_SERIAL_PORT_SERVICE_IPP
#define NETIO_DETAIL_IMPL_WIN_IOCP_SERIAL_PORT_SERVICE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP) && defined(NETIO_HAS_SERIAL_PORT)

#include <cstring>
#include "netio/detail/win_iocp_serial_port_service.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

win_iocp_serial_port_service::win_iocp_serial_port_service(
    execution_context& context)
  : execution_context_service_base<win_iocp_serial_port_service>(context),
    handle_service_(context)
{
}

void win_iocp_serial_port_service::shutdown()
{
}

netio::error_code win_iocp_serial_port_service::open(
    win_iocp_serial_port_service::implementation_type& impl,
    const std::string& device, netio::error_code& ec)
{
  if (is_open(impl))
  {
    ec = netio::error::already_open;
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // For convenience, add a leading \\.\ sequence if not already present.
  std::string name = (device[0] == '\\') ? device : "\\\\.\\" + device;

  // Open a handle to the serial port.
  ::HANDLE handle = ::CreateFileA(name.c_str(),
      GENERIC_READ | GENERIC_WRITE, 0, 0,
      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
  if (handle == INVALID_HANDLE_VALUE)
  {
    DWORD last_error = ::GetLastError();
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Determine the initial serial port parameters.
  using namespace std; // For memset.
  ::DCB dcb;
  memset(&dcb, 0, sizeof(DCB));
  dcb.DCBlength = sizeof(DCB);
  if (!::GetCommState(handle, &dcb))
  {
    DWORD last_error = ::GetLastError();
    ::CloseHandle(handle);
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Set some default serial port parameters. This implementation does not
  // support changing all of these, so they might as well be in a known state.
  dcb.fBinary = TRUE; // Win32 only supports binary mode.
  dcb.fNull = FALSE; // Do not ignore NULL characters.
  dcb.fAbortOnError = FALSE; // Ignore serial framing errors.
  dcb.BaudRate = CBR_9600; // 9600 baud by default
  dcb.ByteSize = 8; // 8 bit bytes
  dcb.fOutxCtsFlow = FALSE; // No flow control
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fParity = FALSE; // No parity
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT; // One stop bit
  if (!::SetCommState(handle, &dcb))
  {
    DWORD last_error = ::GetLastError();
    ::CloseHandle(handle);
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // Set up timeouts so that the serial port will behave similarly to a
  // network socket. Reads wait for at least one byte, then return with
  // whatever they have. Writes return once everything is out the door.
  ::COMMTIMEOUTS timeouts;
  timeouts.ReadIntervalTimeout = 1;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;
  if (!::SetCommTimeouts(handle, &timeouts))
  {
    DWORD last_error = ::GetLastError();
    ::CloseHandle(handle);
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  // We're done. Take ownership of the serial port handle.
  if (handle_service_.assign(impl, handle, ec))
    ::CloseHandle(handle);
  return ec;
}

netio::error_code win_iocp_serial_port_service::do_set_option(
    win_iocp_serial_port_service::implementation_type& impl,
    win_iocp_serial_port_service::store_function_type store,
    const void* option, netio::error_code& ec)
{
  using namespace std; // For memcpy.

  ::DCB dcb;
  memset(&dcb, 0, sizeof(DCB));
  dcb.DCBlength = sizeof(DCB);
  if (!::GetCommState(handle_service_.native_handle(impl), &dcb))
  {
    DWORD last_error = ::GetLastError();
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  if (store(option, dcb, ec))
    return ec;

  if (!::SetCommState(handle_service_.native_handle(impl), &dcb))
  {
    DWORD last_error = ::GetLastError();
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  ec = netio::error_code();
  return ec;
}

netio::error_code win_iocp_serial_port_service::do_get_option(
    const win_iocp_serial_port_service::implementation_type& impl,
    win_iocp_serial_port_service::load_function_type load,
    void* option, netio::error_code& ec) const
{
  using namespace std; // For memset.

  ::DCB dcb;
  memset(&dcb, 0, sizeof(DCB));
  dcb.DCBlength = sizeof(DCB);
  if (!::GetCommState(handle_service_.native_handle(impl), &dcb))
  {
    DWORD last_error = ::GetLastError();
    ec = netio::error_code(last_error,
        netio::error::get_system_category());
    NETIO_ERROR_LOCATION(ec);
    return ec;
  }

  return load(option, dcb, ec);
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IOCP) && defined(NETIO_HAS_SERIAL_PORT)

#endif // NETIO_DETAIL_IMPL_WIN_IOCP_SERIAL_PORT_SERVICE_IPP
