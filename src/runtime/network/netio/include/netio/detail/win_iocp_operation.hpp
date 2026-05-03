#ifndef NETIO_DETAIL_WIN_IOCP_OPERATION_HPP
#define NETIO_DETAIL_WIN_IOCP_OPERATION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_IOCP)

#include "netio/detail/handler_tracking.hpp"
#include "netio/detail/op_queue.hpp"
#include "netio/detail/socket_types.hpp"
#include "netio/error_code.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class win_iocp_io_context;

// Base class for all operations. A function pointer is used instead of virtual
// functions to avoid the associated overhead.
class win_iocp_operation
  : public OVERLAPPED
    NETIO_ALSO_INHERIT_TRACKED_HANDLER
{
public:
  typedef win_iocp_operation operation_type;

  void complete(void* owner, const netio::error_code& ec,
      std::size_t bytes_transferred)
  {
    func_(owner, this, ec, bytes_transferred);
  }

  void destroy()
  {
    func_(0, this, netio::error_code(), 0);
  }

  void reset()
  {
    Internal = 0;
    InternalHigh = 0;
    Offset = 0;
    OffsetHigh = 0;
    hEvent = 0;
    ready_ = 0;
  }

protected:
  typedef void (*func_type)(
      void*, win_iocp_operation*,
      const netio::error_code&, std::size_t);

  win_iocp_operation(func_type func)
    : next_(0),
      func_(func)
  {
    reset();
  }

  // Prevents deletion through this type.
  ~win_iocp_operation()
  {
  }

private:
  friend class op_queue_access;
  friend class win_iocp_io_context;
  win_iocp_operation* next_;
  func_type func_;
  long ready_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_IOCP)

#endif // NETIO_DETAIL_WIN_IOCP_OPERATION_HPP
