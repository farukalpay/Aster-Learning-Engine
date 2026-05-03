#ifndef NETIO_IMPL_CONNECT_PIPE_HPP
#define NETIO_IMPL_CONNECT_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PIPE)

#include "netio/connect_pipe.hpp"
#include "netio/detail/throw_error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

template <typename Executor1, typename Executor2>
void connect_pipe(basic_readable_pipe<Executor1>& read_end,
    basic_writable_pipe<Executor2>& write_end)
{
  netio::error_code ec;
  netio::connect_pipe(read_end, write_end, ec);
  netio::detail::throw_error(ec, "connect_pipe");
}

template <typename Executor1, typename Executor2>
NETIO_SYNC_OP_VOID connect_pipe(basic_readable_pipe<Executor1>& read_end,
    basic_writable_pipe<Executor2>& write_end, netio::error_code& ec)
{
  detail::native_pipe_handle p[2];
  detail::create_pipe(p, ec);
  if (ec)
    NETIO_SYNC_OP_VOID_RETURN(ec);

  read_end.assign(p[0], ec);
  if (ec)
  {
    detail::close_pipe(p[0]);
    detail::close_pipe(p[1]);
    NETIO_SYNC_OP_VOID_RETURN(ec);
  }

  write_end.assign(p[1], ec);
  if (ec)
  {
    netio::error_code temp_ec;
    read_end.close(temp_ec);
    detail::close_pipe(p[1]);
    NETIO_SYNC_OP_VOID_RETURN(ec);
  }

  NETIO_SYNC_OP_VOID_RETURN(ec);
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_PIPE)

#endif // NETIO_IMPL_CONNECT_PIPE_HPP
