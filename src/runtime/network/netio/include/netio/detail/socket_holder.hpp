#ifndef NETIO_DETAIL_SOCKET_HOLDER_HPP
#define NETIO_DETAIL_SOCKET_HOLDER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/socket_ops.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

// Implement the resource acquisition is initialisation idiom for sockets.
class socket_holder
  : private noncopyable
{
public:
  // Construct as an uninitialised socket.
  socket_holder()
    : socket_(invalid_socket)
  {
  }

  // Construct to take ownership of the specified socket.
  explicit socket_holder(socket_type s)
    : socket_(s)
  {
  }

  // Destructor.
  ~socket_holder()
  {
    if (socket_ != invalid_socket)
    {
      netio::error_code ec;
      socket_ops::state_type state = 0;
      socket_ops::close(socket_, state, true, ec);
    }
  }

  // Get the underlying socket.
  socket_type get() const
  {
    return socket_;
  }

  // Reset to an uninitialised socket.
  void reset()
  {
    if (socket_ != invalid_socket)
    {
      netio::error_code ec;
      socket_ops::state_type state = 0;
      socket_ops::close(socket_, state, true, ec);
      socket_ = invalid_socket;
    }
  }

  // Reset to take ownership of the specified socket.
  void reset(socket_type s)
  {
    reset();
    socket_ = s;
  }

  // Release ownership of the socket.
  socket_type release()
  {
    socket_type tmp = socket_;
    socket_ = invalid_socket;
    return tmp;
  }

private:
  // The underlying socket.
  socket_type socket_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_SOCKET_HOLDER_HPP
