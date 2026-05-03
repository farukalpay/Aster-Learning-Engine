#ifndef NETIO_CONNECT_PIPE_HPP
#define NETIO_CONNECT_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PIPE) \
  || defined(GENERATING_DOCUMENTATION)

#include "netio/basic_readable_pipe.hpp"
#include "netio/basic_writable_pipe.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

#if defined(NETIO_HAS_IOCP)
typedef HANDLE native_pipe_handle;
#else // defined(NETIO_HAS_IOCP)
typedef int native_pipe_handle;
#endif // defined(NETIO_HAS_IOCP)

NETIO_DECL void create_pipe(native_pipe_handle p[2],
    netio::error_code& ec);

NETIO_DECL void close_pipe(native_pipe_handle p);

} // namespace detail

/// Connect two pipe ends using an anonymous pipe.
/**
 * @param read_end The read end of the pipe.
 *
 * @param write_end The write end of the pipe.
 *
 * @throws netio::system_error Thrown on failure.
 */
template <typename Executor1, typename Executor2>
void connect_pipe(basic_readable_pipe<Executor1>& read_end,
    basic_writable_pipe<Executor2>& write_end);

/// Connect two pipe ends using an anonymous pipe.
/**
 * @param read_end The read end of the pipe.
 *
 * @param write_end The write end of the pipe.
 *
 * @throws netio::system_error Thrown on failure.
 *
 * @param ec Set to indicate what error occurred, if any.
 */
template <typename Executor1, typename Executor2>
NETIO_SYNC_OP_VOID connect_pipe(basic_readable_pipe<Executor1>& read_end,
    basic_writable_pipe<Executor2>& write_end, netio::error_code& ec);

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/connect_pipe.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/impl/connect_pipe.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_PIPE)
       //   || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_CONNECT_PIPE_HPP
