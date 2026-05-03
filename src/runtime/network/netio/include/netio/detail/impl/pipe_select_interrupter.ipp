#ifndef NETIO_DETAIL_IMPL_PIPE_SELECT_INTERRUPTER_IPP
#define NETIO_DETAIL_IMPL_PIPE_SELECT_INTERRUPTER_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_WINDOWS_RUNTIME)
#if !defined(NETIO_WINDOWS)
#if !defined(__CYGWIN__)
#if !defined(__SYMBIAN32__)
#if !defined(NETIO_HAS_EVENTFD)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "netio/detail/pipe_select_interrupter.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

pipe_select_interrupter::pipe_select_interrupter(bool)
{
  open_descriptors();
}

void pipe_select_interrupter::open_descriptors()
{
  int pipe_fds[2];
  if (pipe(pipe_fds) == 0)
  {
    read_descriptor_ = pipe_fds[0];
    ::fcntl(read_descriptor_, F_SETFL, O_NONBLOCK);
    write_descriptor_ = pipe_fds[1];
    ::fcntl(write_descriptor_, F_SETFL, O_NONBLOCK);

#if defined(FD_CLOEXEC)
    ::fcntl(read_descriptor_, F_SETFD, FD_CLOEXEC);
    ::fcntl(write_descriptor_, F_SETFD, FD_CLOEXEC);
#endif // defined(FD_CLOEXEC)
  }
  else
  {
    netio::error_code ec(errno,
        netio::error::get_system_category());
    netio::detail::throw_error(ec, "pipe_select_interrupter");
  }
}

pipe_select_interrupter::~pipe_select_interrupter()
{
  close_descriptors();
}

void pipe_select_interrupter::close_descriptors()
{
  if (read_descriptor_ != -1)
    ::close(read_descriptor_);
  if (write_descriptor_ != -1)
    ::close(write_descriptor_);
}

void pipe_select_interrupter::recreate()
{
  close_descriptors();

  write_descriptor_ = -1;
  read_descriptor_ = -1;

  open_descriptors();
}

void pipe_select_interrupter::interrupt()
{
  char byte = 0;
  signed_size_type result = ::write(write_descriptor_, &byte, 1);
  (void)result;
}

bool pipe_select_interrupter::reset()
{
  for (;;)
  {
    char data[1024];
    signed_size_type bytes_read = ::read(read_descriptor_, data, sizeof(data));
    if (bytes_read == sizeof(data))
      continue;
    if (bytes_read > 0)
      return true;
    if (bytes_read == 0)
      return false;
    if (errno == EINTR)
      continue;
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return true;
    return false;
  }
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_HAS_EVENTFD)
#endif // !defined(__SYMBIAN32__)
#endif // !defined(__CYGWIN__)
#endif // !defined(NETIO_WINDOWS)
#endif // !defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_IMPL_PIPE_SELECT_INTERRUPTER_IPP
