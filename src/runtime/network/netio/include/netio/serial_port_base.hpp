#ifndef NETIO_SERIAL_PORT_BASE_HPP
#define NETIO_SERIAL_PORT_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_SERIAL_PORT) \
  || defined(GENERATING_DOCUMENTATION)

#if !defined(NETIO_WINDOWS) && !defined(__CYGWIN__)
# include <termios.h>
#endif // !defined(NETIO_WINDOWS) && !defined(__CYGWIN__)

#include "netio/detail/socket_types.hpp"
#include "netio/error_code.hpp"

#if defined(GENERATING_DOCUMENTATION)
# define NETIO_OPTION_STORAGE implementation_defined
#elif defined(NETIO_WINDOWS) || defined(__CYGWIN__)
# define NETIO_OPTION_STORAGE DCB
#else
# define NETIO_OPTION_STORAGE termios
#endif

#include "netio/detail/push_options.hpp"

namespace netio {

/// The serial_port_base class is used as a base for the basic_serial_port class
/// template so that we have a common place to define the serial port options.
class serial_port_base
{
public:
  /// Serial port option to permit changing the baud rate.
  /**
   * Implements changing the baud rate for a given serial port.
   */
  class baud_rate
  {
  public:
    explicit baud_rate(unsigned int rate = 0);
    unsigned int value() const;
    NETIO_DECL NETIO_SYNC_OP_VOID store(
        NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec) const;
    NETIO_DECL NETIO_SYNC_OP_VOID load(
        const NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec);
  private:
    unsigned int value_;
  };

  /// Serial port option to permit changing the flow control.
  /**
   * Implements changing the flow control for a given serial port.
   */
  class flow_control
  {
  public:
    enum type { none, software, hardware };
    NETIO_DECL explicit flow_control(type t = none);
    type value() const;
    NETIO_DECL NETIO_SYNC_OP_VOID store(
        NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec) const;
    NETIO_DECL NETIO_SYNC_OP_VOID load(
        const NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec);
  private:
    type value_;
  };

  /// Serial port option to permit changing the parity.
  /**
   * Implements changing the parity for a given serial port.
   */
  class parity
  {
  public:
    enum type { none, odd, even };
    NETIO_DECL explicit parity(type t = none);
    type value() const;
    NETIO_DECL NETIO_SYNC_OP_VOID store(
        NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec) const;
    NETIO_DECL NETIO_SYNC_OP_VOID load(
        const NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec);
  private:
    type value_;
  };

  /// Serial port option to permit changing the number of stop bits.
  /**
   * Implements changing the number of stop bits for a given serial port.
   */
  class stop_bits
  {
  public:
    enum type { one, onepointfive, two };
    NETIO_DECL explicit stop_bits(type t = one);
    type value() const;
    NETIO_DECL NETIO_SYNC_OP_VOID store(
        NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec) const;
    NETIO_DECL NETIO_SYNC_OP_VOID load(
        const NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec);
  private:
    type value_;
  };

  /// Serial port option to permit changing the character size.
  /**
   * Implements changing the character size for a given serial port.
   */
  class character_size
  {
  public:
    NETIO_DECL explicit character_size(unsigned int t = 8);
    unsigned int value() const;
    NETIO_DECL NETIO_SYNC_OP_VOID store(
        NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec) const;
    NETIO_DECL NETIO_SYNC_OP_VOID load(
        const NETIO_OPTION_STORAGE& storage,
        netio::error_code& ec);
  private:
    unsigned int value_;
  };

protected:
  /// Protected destructor to prevent deletion through this type.
  ~serial_port_base()
  {
  }
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#undef NETIO_OPTION_STORAGE

#include "netio/impl/serial_port_base.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/impl/serial_port_base.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_SERIAL_PORT)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_SERIAL_PORT_BASE_HPP
