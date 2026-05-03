#ifndef NETIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP
#define NETIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <cerrno>
#include "netio/detail/descriptor_ops.hpp"
#include "netio/error.hpp"

#if !defined(NETIO_WINDOWS) \
  && !defined(NETIO_WINDOWS_RUNTIME) \
  && !defined(__CYGWIN__)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {
namespace descriptor_ops {

int open(const char* path, int flags, netio::error_code& ec)
{
  int result = ::open(path, flags);
  get_last_error(ec, result < 0);
  return result;
}

int open(const char* path, int flags,
    unsigned mode, netio::error_code& ec)
{
  int result = ::open(path, flags, mode);
  get_last_error(ec, result < 0);
  return result;
}

int close(int d, state_type& state, netio::error_code& ec)
{
  int result = 0;
  if (d != -1)
  {
    result = ::close(d);
    get_last_error(ec, result < 0);

    if (result != 0
        && (ec == netio::error::would_block
          || ec == netio::error::try_again))
    {
      // According to UNIX Network Programming Vol. 1, it is possible for
      // close() to fail with EWOULDBLOCK under certain circumstances. What
      // isn't clear is the state of the descriptor after this error. The one
      // current OS where this behaviour is seen, Windows, says that the socket
      // remains open. Therefore we'll put the descriptor back into blocking
      // mode and have another attempt at closing it.
#if defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
      int flags = ::fcntl(d, F_GETFL, 0);
      if (flags >= 0)
        ::fcntl(d, F_SETFL, flags & ~O_NONBLOCK);
#else // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
      ioctl_arg_type arg = 0;
      if ((state & possible_dup) == 0)
      {
        result = ::ioctl(d, FIONBIO, &arg);
        get_last_error(ec, result < 0);
      }
      if ((state & possible_dup) != 0
# if defined(ENOTTY)
          || ec.value() == ENOTTY
# endif // defined(ENOTTY)
# if defined(ENOTCAPABLE)
          || ec.value() == ENOTCAPABLE
# endif // defined(ENOTCAPABLE)
        )
      {
        int flags = ::fcntl(d, F_GETFL, 0);
        if (flags >= 0)
          ::fcntl(d, F_SETFL, flags & ~O_NONBLOCK);
      }
#endif // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
      state &= ~non_blocking;

      result = ::close(d);
      get_last_error(ec, result < 0);
    }
  }

  return result;
}

bool set_user_non_blocking(int d, state_type& state,
    bool value, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return false;
  }

#if defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
  int result = ::fcntl(d, F_GETFL, 0);
  get_last_error(ec, result < 0);
  if (result >= 0)
  {
    int flag = (value ? (result | O_NONBLOCK) : (result & ~O_NONBLOCK));
    result = (flag != result) ? ::fcntl(d, F_SETFL, flag) : 0;
    get_last_error(ec, result < 0);
  }
#else // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
  ioctl_arg_type arg = (value ? 1 : 0);
  int result = 0;
  if ((state & possible_dup) == 0)
  {
    result = ::ioctl(d, FIONBIO, &arg);
    get_last_error(ec, result < 0);
  }
  if ((state & possible_dup) != 0
# if defined(ENOTTY)
      || ec.value() == ENOTTY
# endif // defined(ENOTTY)
# if defined(ENOTCAPABLE)
      || ec.value() == ENOTCAPABLE
# endif // defined(ENOTCAPABLE)
    )
  {
    result = ::fcntl(d, F_GETFL, 0);
    get_last_error(ec, result < 0);
    if (result >= 0)
    {
      int flag = (value ? (result | O_NONBLOCK) : (result & ~O_NONBLOCK));
      result = (flag != result) ? ::fcntl(d, F_SETFL, flag) : 0;
      get_last_error(ec, result < 0);
    }
  }
#endif // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)

  if (result >= 0)
  {
    if (value)
      state |= user_set_non_blocking;
    else
    {
      // Clearing the user-set non-blocking mode always overrides any
      // internally-set non-blocking flag. Any subsequent asynchronous
      // operations will need to re-enable non-blocking I/O.
      state &= ~(user_set_non_blocking | internal_non_blocking);
    }
    return true;
  }

  return false;
}

bool set_internal_non_blocking(int d, state_type& state,
    bool value, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return false;
  }

  if (!value && (state & user_set_non_blocking))
  {
    // It does not make sense to clear the internal non-blocking flag if the
    // user still wants non-blocking behaviour. Return an error and let the
    // caller figure out whether to update the user-set non-blocking flag.
    ec = netio::error::invalid_argument;
    return false;
  }

#if defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
  int result = ::fcntl(d, F_GETFL, 0);
  get_last_error(ec, result < 0);
  if (result >= 0)
  {
    int flag = (value ? (result | O_NONBLOCK) : (result & ~O_NONBLOCK));
    result = (flag != result) ? ::fcntl(d, F_SETFL, flag) : 0;
    get_last_error(ec, result < 0);
  }
#else // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)
  ioctl_arg_type arg = (value ? 1 : 0);
  int result = 0;
  if ((state & possible_dup) == 0)
  {
    result = ::ioctl(d, FIONBIO, &arg);
    get_last_error(ec, result < 0);
  }
  if ((state & possible_dup) != 0
# if defined(ENOTTY)
      || ec.value() == ENOTTY
# endif // defined(ENOTTY)
# if defined(ENOTCAPABLE)
      || ec.value() == ENOTCAPABLE
# endif // defined(ENOTCAPABLE)
    )
  {
    result = ::fcntl(d, F_GETFL, 0);
    get_last_error(ec, result < 0);
    if (result >= 0)
    {
      int flag = (value ? (result | O_NONBLOCK) : (result & ~O_NONBLOCK));
      result = (flag != result) ? ::fcntl(d, F_SETFL, flag) : 0;
      get_last_error(ec, result < 0);
    }
  }
#endif // defined(__SYMBIAN32__) || defined(__EMSCRIPTEN__)

  if (result >= 0)
  {
    if (value)
      state |= internal_non_blocking;
    else
      state &= ~internal_non_blocking;
    return true;
  }

  return false;
}

std::size_t sync_read(int d, state_type state, buf* bufs,
    std::size_t count, bool all_empty, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::readv(d, bufs, static_cast<int>(count));
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return 0;
    }

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_read(d, 0, ec) < 0)
      return 0;
  }
}

std::size_t sync_read1(int d, state_type state, void* data,
    std::size_t size, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (size == 0)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::read(d, data, size);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return 0;
    }

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_read(d, 0, ec) < 0)
      return 0;
  }
}

bool non_blocking_read(int d, buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = ::readv(d, bufs, static_cast<int>(count));
    get_last_error(ec, bytes < 0);

    // Check for end of stream.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return true;
    }

    // Check if operation succeeded.
    if (bytes > 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

bool non_blocking_read1(int d, void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = ::read(d, data, size);
    get_last_error(ec, bytes < 0);

    // Check for end of stream.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return true;
    }

    // Check if operation succeeded.
    if (bytes > 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

std::size_t sync_write(int d, state_type state, const buf* bufs,
    std::size_t count, bool all_empty, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to write 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Write some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::writev(d, bufs, static_cast<int>(count));
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_write(d, 0, ec) < 0)
      return 0;
  }
}

std::size_t sync_write1(int d, state_type state, const void* data,
    std::size_t size, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to write 0 bytes on a stream is a no-op.
  if (size == 0)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Write some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::write(d, data, size);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_write(d, 0, ec) < 0)
      return 0;
  }
}

bool non_blocking_write(int d, const buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Write some data.
    signed_size_type bytes = ::writev(d, bufs, static_cast<int>(count));
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes >= 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

bool non_blocking_write1(int d, const void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Write some data.
    signed_size_type bytes = ::write(d, data, size);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes >= 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

#if defined(NETIO_HAS_FILE)

std::size_t sync_read_at(int d, state_type state, uint64_t offset,
    buf* bufs, std::size_t count, bool all_empty, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::preadv(d, bufs, static_cast<int>(count), offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return 0;
    }

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_read(d, 0, ec) < 0)
      return 0;
  }
}

std::size_t sync_read_at1(int d, state_type state, uint64_t offset,
    void* data, std::size_t size, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (size == 0)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::pread(d, data, size, offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return 0;
    }

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_read(d, 0, ec) < 0)
      return 0;
  }
}

bool non_blocking_read_at(int d, uint64_t offset, buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = ::preadv(d, bufs, static_cast<int>(count), offset);
    get_last_error(ec, bytes < 0);

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return true;
    }

    // Check if operation succeeded.
    if (bytes > 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

bool non_blocking_read_at1(int d, uint64_t offset, void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = ::pread(d, data, size, offset);
    get_last_error(ec, bytes < 0);

    // Check for EOF.
    if (bytes == 0)
    {
      ec = netio::error::eof;
      return true;
    }

    // Check if operation succeeded.
    if (bytes > 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

std::size_t sync_write_at(int d, state_type state, uint64_t offset,
    const buf* bufs, std::size_t count, bool all_empty,
    netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to write 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Write some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::pwritev(d,
        bufs, static_cast<int>(count), offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_write(d, 0, ec) < 0)
      return 0;
  }
}

std::size_t sync_write_at1(int d, state_type state, uint64_t offset,
    const void* data, std::size_t size, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return 0;
  }

  // A request to write 0 bytes on a stream is a no-op.
  if (size == 0)
  {
    netio::error::clear(ec);
    return 0;
  }

  // Write some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = ::pwrite(d, data, size, offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != netio::error::would_block
          && ec != netio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_write(d, 0, ec) < 0)
      return 0;
  }
}

bool non_blocking_write_at(int d, uint64_t offset,
    const buf* bufs, std::size_t count,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Write some data.
    signed_size_type bytes = ::pwritev(d,
        bufs, static_cast<int>(count), offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes >= 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

bool non_blocking_write_at1(int d, uint64_t offset,
    const void* data, std::size_t size,
    netio::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Write some data.
    signed_size_type bytes = ::pwrite(d, data, size, offset);
    get_last_error(ec, bytes < 0);

    // Check if operation succeeded.
    if (bytes >= 0)
    {
      bytes_transferred = bytes;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == netio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == netio::error::would_block
        || ec == netio::error::try_again)
      return false;

    // Operation failed.
    bytes_transferred = 0;
    return true;
  }
}

#endif // defined(NETIO_HAS_FILE)

int ioctl(int d, state_type& state, long cmd,
    ioctl_arg_type* arg, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  int result = ::ioctl(d, cmd, arg);
  get_last_error(ec, result < 0);

  if (result >= 0)
  {
    // When updating the non-blocking mode we always perform the ioctl syscall,
    // even if the flags would otherwise indicate that the descriptor is
    // already in the correct state. This ensures that the underlying
    // descriptor is put into the state that has been requested by the user. If
    // the ioctl syscall was successful then we need to update the flags to
    // match.
    if (cmd == static_cast<long>(FIONBIO))
    {
      if (*arg)
      {
        state |= user_set_non_blocking;
      }
      else
      {
        // Clearing the non-blocking mode always overrides any internally-set
        // non-blocking flag. Any subsequent asynchronous operations will need
        // to re-enable non-blocking I/O.
        state &= ~(user_set_non_blocking | internal_non_blocking);
      }
    }
  }

  return result;
}

int fcntl(int d, int cmd, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  int result = ::fcntl(d, cmd);
  get_last_error(ec, result < 0);
  return result;
}

int fcntl(int d, int cmd, long arg, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  int result = ::fcntl(d, cmd, arg);
  get_last_error(ec, result < 0);
  return result;
}

int poll_read(int d, state_type state, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  pollfd fds;
  fds.fd = d;
  fds.events = POLLIN;
  fds.revents = 0;
  int timeout = (state & user_set_non_blocking) ? 0 : -1;
  int result = ::poll(&fds, 1, timeout);
  get_last_error(ec, result < 0);
  if (result == 0)
    if (state & user_set_non_blocking)
      ec = netio::error::would_block;
  return result;
}

int poll_write(int d, state_type state, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  pollfd fds;
  fds.fd = d;
  fds.events = POLLOUT;
  fds.revents = 0;
  int timeout = (state & user_set_non_blocking) ? 0 : -1;
  int result = ::poll(&fds, 1, timeout);
  get_last_error(ec, result < 0);
  if (result == 0)
    if (state & user_set_non_blocking)
      ec = netio::error::would_block;
  return result;
}

int poll_error(int d, state_type state, netio::error_code& ec)
{
  if (d == -1)
  {
    ec = netio::error::bad_descriptor;
    return -1;
  }

  pollfd fds;
  fds.fd = d;
  fds.events = POLLPRI | POLLERR | POLLHUP;
  fds.revents = 0;
  int timeout = (state & user_set_non_blocking) ? 0 : -1;
  int result = ::poll(&fds, 1, timeout);
  get_last_error(ec, result < 0);
  if (result == 0)
    if (state & user_set_non_blocking)
      ec = netio::error::would_block;
  return result;
}

} // namespace descriptor_ops
} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_WINDOWS)
       //   && !defined(NETIO_WINDOWS_RUNTIME)
       //   && !defined(__CYGWIN__)

#endif // NETIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP
