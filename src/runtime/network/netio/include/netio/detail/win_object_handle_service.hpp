#ifndef NETIO_DETAIL_WIN_OBJECT_HANDLE_SERVICE_HPP
#define NETIO_DETAIL_WIN_OBJECT_HANDLE_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_WINDOWS_OBJECT_HANDLE)

#include "netio/detail/memory.hpp"
#include "netio/detail/wait_handler.hpp"
#include "netio/error.hpp"
#include "netio/execution_context.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else // defined(NETIO_HAS_IOCP)
# include "netio/detail/scheduler.hpp"
#endif // defined(NETIO_HAS_IOCP)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class win_object_handle_service :
  public execution_context_service_base<win_object_handle_service>
{
public:
  // The native type of an object handle.
  typedef HANDLE native_handle_type;

  // The implementation type of the object handle.
  class implementation_type
  {
   public:
    // Default constructor.
    implementation_type()
      : handle_(INVALID_HANDLE_VALUE),
        wait_handle_(INVALID_HANDLE_VALUE),
        owner_(0),
        next_(0),
        prev_(0)
    {
    }

  private:
    // Only this service will have access to the internal values.
    friend class win_object_handle_service;

    // The native object handle representation. May be accessed or modified
    // without locking the mutex.
    native_handle_type handle_;

    // The handle used to unregister the wait operation. The mutex must be
    // locked when accessing or modifying this member.
    HANDLE wait_handle_;

    // The operations waiting on the object handle. If there is a registered
    // wait then the mutex must be locked when accessing or modifying this
    // member
    op_queue<wait_op> op_queue_;

    // The service instance that owns the object handle implementation.
    win_object_handle_service* owner_;

    // Pointers to adjacent handle implementations in linked list. The mutex
    // must be locked when accessing or modifying these members.
    implementation_type* next_;
    implementation_type* prev_;
  };

  // Constructor.
  NETIO_DECL win_object_handle_service(execution_context& context);

  // Destroy all user-defined handler objects owned by the service.
  NETIO_DECL void shutdown();

  // Construct a new handle implementation.
  NETIO_DECL void construct(implementation_type& impl);

  // Move-construct a new handle implementation.
  NETIO_DECL void move_construct(implementation_type& impl,
      implementation_type& other_impl);

  // Move-assign from another handle implementation.
  NETIO_DECL void move_assign(implementation_type& impl,
      win_object_handle_service& other_service,
      implementation_type& other_impl);

  // Destroy a handle implementation.
  NETIO_DECL void destroy(implementation_type& impl);

  // Assign a native handle to a handle implementation.
  NETIO_DECL netio::error_code assign(implementation_type& impl,
      const native_handle_type& handle, netio::error_code& ec);

  // Determine whether the handle is open.
  bool is_open(const implementation_type& impl) const
  {
    return impl.handle_ != INVALID_HANDLE_VALUE && impl.handle_ != 0;
  }

  // Destroy a handle implementation.
  NETIO_DECL netio::error_code close(implementation_type& impl,
      netio::error_code& ec);

  // Get the native handle representation.
  native_handle_type native_handle(const implementation_type& impl) const
  {
    return impl.handle_;
  }

  // Cancel all operations associated with the handle.
  NETIO_DECL netio::error_code cancel(implementation_type& impl,
      netio::error_code& ec);

  // Perform a synchronous wait for the object to enter a signalled state.
  NETIO_DECL void wait(implementation_type& impl,
      netio::error_code& ec);

  /// Start an asynchronous wait.
  template <typename Handler, typename IoExecutor>
  void async_wait(implementation_type& impl,
      Handler& handler, const IoExecutor& io_ex)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef wait_handler<Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(handler, io_ex);

    NETIO_HANDLER_CREATION((scheduler_.context(), *p.p, "object_handle",
          &impl, reinterpret_cast<uintmax_t>(impl.wait_handle_), "async_wait"));

    start_wait_op(impl, p.p);
    p.v = p.p = 0;
  }

private:
  // Helper function to start an asynchronous wait operation.
  NETIO_DECL void start_wait_op(implementation_type& impl, wait_op* op);

  // Helper function to register a wait operation.
  NETIO_DECL void register_wait_callback(
      implementation_type& impl, mutex::scoped_lock& lock);

  // Callback function invoked when the registered wait completes.
  static NETIO_DECL VOID CALLBACK wait_callback(
      PVOID param, BOOLEAN timeout);

  // The scheduler used to post completions.
#if defined(NETIO_HAS_IOCP)
  typedef class win_iocp_io_context scheduler_impl;
#else
  typedef class scheduler scheduler_impl;
#endif
  scheduler_impl& scheduler_;

  // Mutex to protect access to internal state.
  mutex mutex_;

  // The head of a linked list of all implementations.
  implementation_type* impl_list_;

  // Flag to indicate that the dispatcher has been shut down.
  bool shutdown_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/win_object_handle_service.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_WINDOWS_OBJECT_HANDLE)

#endif // NETIO_DETAIL_WIN_OBJECT_HANDLE_SERVICE_HPP
