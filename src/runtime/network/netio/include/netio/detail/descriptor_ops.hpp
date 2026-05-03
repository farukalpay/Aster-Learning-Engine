#ifndef NETIO_DETAIL_DESCRIPTOR_OPS_HPP
#define NETIO_DETAIL_DESCRIPTOR_OPS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS) \
  && !defined(NETIO_WINDOWS_RUNTIME) \
  && !defined(__CYGWIN__)

#include <cstddef>
#include "netio/error.hpp"
#include "netio/error_code.hpp"
#include "netio/detail/cstdint.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {
namespace descriptor_ops {

// Descriptor state bits.
enum
{
  // The user wants a non-blocking descriptor.
  user_set_non_blocking = 1,

  // The descriptor has been set non-blocking.
  internal_non_blocking = 2,

  // Helper "state" used to determine whether the descriptor is non-blocking.
  non_blocking = user_set_non_blocking | internal_non_blocking,

  // The descriptor may have been dup()-ed.
  possible_dup = 4
};

typedef unsigned char state_type;

inline void get_last_error(
    netio::error_code& ec, bool is_error_condition)
{
  if (!is_error_condition)
  {
    netio::error::clear(ec);
  }
  else
  {
    ec = netio::error_code(errno,
        netio::error::get_system_category());
  }
}

NETIO_DECL int open(const char* path, int flags,
    netio::error_code& ec);

NETIO_DECL int open(const char* path, int flags, unsigned mode,
    netio::error_code& ec);

NETIO_DECL int close(int d, state_type& state,
    netio::error_code& ec);

NETIO_DECL bool set_user_non_blocking(int d,
    state_type& state, bool value, netio::error_code& ec);

NETIO_DECL bool set_internal_non_blocking(int d,
    state_type& state, bool value, netio::error_code& ec);

typedef iovec buf;

NETIO_DECL std::size_t sync_read(int d, state_type state, buf* bufs,
    std::size_t count, bool all_empty, netio::error_code& ec);

NETIO_DECL std::size_t sync_read1(int d, state_type state, void* data,
    std::size_t size, netio::error_code& ec);

NETIO_DECL bool non_blocking_read(int d, buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred);

NETIO_DECL bool non_blocking_read1(int d, void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred);

NETIO_DECL std::size_t sync_write(int d, state_type state,
    const buf* bufs, std::size_t count, bool all_empty,
    netio::error_code& ec);

NETIO_DECL std::size_t sync_write1(int d, state_type state,
    const void* data, std::size_t size, netio::error_code& ec);

NETIO_DECL bool non_blocking_write(int d,
    const buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred);

NETIO_DECL bool non_blocking_write1(int d,
    const void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred);

#if defined(NETIO_HAS_FILE)

NETIO_DECL std::size_t sync_read_at(int d, state_type state,
    uint64_t offset, buf* bufs, std::size_t count, bool all_empty,
    netio::error_code& ec);

NETIO_DECL std::size_t sync_read_at1(int d, state_type state,
    uint64_t offset, void* data, std::size_t size,
    netio::error_code& ec);

NETIO_DECL bool non_blocking_read_at(int d, uint64_t offset,
    buf* bufs, std::size_t count, netio::error_code& ec,
    std::size_t& bytes_transferred);

NETIO_DECL bool non_blocking_read_at1(int d, uint64_t offset,
    void* data, std::size_t size, netio::error_code& ec,
    std::size_t& bytes_transferred);

NETIO_DECL std::size_t sync_write_at(int d, state_type state,
    uint64_t offset, const buf* bufs, std::size_t count, bool all_empty,
    netio::error_code& ec);

NETIO_DECL std::size_t sync_write_at1(int d, state_type state,
    uint64_t offset, const void* data, std::size_t size,
    netio::error_code& ec);

NETIO_DECL bool non_blocking_write_at(int d,
    uint64_t offset, const buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred);

NETIO_DECL bool non_blocking_write_at1(int d,
    uint64_t offset, const void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred);

#endif // defined(NETIO_HAS_FILE)

NETIO_DECL int ioctl(int d, state_type& state, long cmd,
    ioctl_arg_type* arg, netio::error_code& ec);

NETIO_DECL int fcntl(int d, int cmd, netio::error_code& ec);

NETIO_DECL int fcntl(int d, int cmd,
    long arg, netio::error_code& ec);

NETIO_DECL int poll_read(int d,
    state_type state, netio::error_code& ec);

NETIO_DECL int poll_write(int d,
    state_type state, netio::error_code& ec);

NETIO_DECL int poll_error(int d,
    state_type state, netio::error_code& ec);

} // namespace descriptor_ops
} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/descriptor_ops.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // !defined(NETIO_WINDOWS)
       //   && !defined(NETIO_WINDOWS_RUNTIME)
       //   && !defined(__CYGWIN__)

#endif // NETIO_DETAIL_DESCRIPTOR_OPS_HPP
