#ifndef NETIO_LOCAL_CONNECT_PAIR_HPP
#define NETIO_LOCAL_CONNECT_PAIR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_LOCAL_SOCKETS) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_socket.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"
#include "netio/local/basic_endpoint.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace local {

/// Create a pair of connected sockets.
template <typename Protocol, typename Executor1, typename Executor2>
void connect_pair(basic_socket<Protocol, Executor1>& socket1,
    basic_socket<Protocol, Executor2>& socket2);

/// Create a pair of connected sockets.
template <typename Protocol, typename Executor1, typename Executor2>
NETIO_SYNC_OP_VOID connect_pair(basic_socket<Protocol, Executor1>& socket1,
    basic_socket<Protocol, Executor2>& socket2, netio::error_code& ec);

template <typename Protocol, typename Executor1, typename Executor2>
inline void connect_pair(basic_socket<Protocol, Executor1>& socket1,
    basic_socket<Protocol, Executor2>& socket2)
{
  netio::error_code ec;
  connect_pair(socket1, socket2, ec);
  netio::detail::throw_error(ec, "connect_pair");
}

template <typename Protocol, typename Executor1, typename Executor2>
inline NETIO_SYNC_OP_VOID connect_pair(
    basic_socket<Protocol, Executor1>& socket1,
    basic_socket<Protocol, Executor2>& socket2, netio::error_code& ec)
{
  // Check that this function is only being used with a UNIX domain socket.
  netio::local::basic_endpoint<Protocol>* tmp
    = static_cast<typename Protocol::endpoint*>(0);
  (void)tmp;

  Protocol protocol;
  netio::detail::socket_type sv[2];
  if (netio::detail::socket_ops::socketpair(protocol.family(),
        protocol.type(), protocol.protocol(), sv, ec)
      == netio::detail::socket_error_retval)
    NETIO_SYNC_OP_VOID_RETURN(ec);

  socket1.assign(protocol, sv[0], ec);
  if (ec)
  {
    netio::error_code temp_ec;
    netio::detail::socket_ops::state_type state[2] = { 0, 0 };
    netio::detail::socket_ops::close(sv[0], state[0], true, temp_ec);
    netio::detail::socket_ops::close(sv[1], state[1], true, temp_ec);
    NETIO_SYNC_OP_VOID_RETURN(ec);
  }

  socket2.assign(protocol, sv[1], ec);
  if (ec)
  {
    netio::error_code temp_ec;
    socket1.close(temp_ec);
    netio::detail::socket_ops::state_type state = 0;
    netio::detail::socket_ops::close(sv[1], state, true, temp_ec);
    NETIO_SYNC_OP_VOID_RETURN(ec);
  }

  NETIO_SYNC_OP_VOID_RETURN(ec);
}

} // namespace local
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_LOCAL_SOCKETS)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_LOCAL_CONNECT_PAIR_HPP
