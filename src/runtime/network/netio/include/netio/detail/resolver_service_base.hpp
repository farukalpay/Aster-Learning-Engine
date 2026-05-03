#ifndef NETIO_DETAIL_RESOLVER_SERVICE_BASE_HPP
#define NETIO_DETAIL_RESOLVER_SERVICE_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/error.hpp"
#include "netio/execution_context.hpp"
#include "netio/detail/noncopyable.hpp"
#include "netio/detail/resolve_op.hpp"
#include "netio/detail/resolver_thread_pool.hpp"
#include "netio/detail/socket_ops.hpp"
#include "netio/detail/socket_types.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else // defined(NETIO_HAS_IOCP)
# include "netio/detail/scheduler.hpp"
#endif // defined(NETIO_HAS_IOCP)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class resolver_service_base
{
public:
  // The implementation type of the resolver. A cancellation token is used to
  // indicate to the background thread that the operation has been cancelled.
  typedef socket_ops::shared_cancel_token_type implementation_type;

  // Constructor.
  NETIO_DECL resolver_service_base(execution_context& context);

  // Destructor.
  NETIO_DECL ~resolver_service_base();

  // Construct a new resolver implementation.
  NETIO_DECL void construct(implementation_type& impl);

  // Destroy a resolver implementation.
  NETIO_DECL void destroy(implementation_type&);

  // Move-construct a new resolver implementation.
  NETIO_DECL void move_construct(implementation_type& impl,
      implementation_type& other_impl);

  // Move-assign from another resolver implementation.
  NETIO_DECL void move_assign(implementation_type& impl,
      resolver_service_base& other_service,
      implementation_type& other_impl);

  // Move-construct a new timer implementation.
  void converting_move_construct(implementation_type& impl,
      resolver_service_base&, implementation_type& other_impl)
  {
    move_construct(impl, other_impl);
  }

  // Move-assign from another timer implementation.
  void converting_move_assign(implementation_type& impl,
      resolver_service_base& other_service,
      implementation_type& other_impl)
  {
    move_assign(impl, other_service, other_impl);
  }

  // Cancel pending asynchronous operations.
  NETIO_DECL void cancel(implementation_type& impl);

protected:
#if !defined(NETIO_WINDOWS_RUNTIME)
  // Helper class to perform exception-safe cleanup of addrinfo objects.
  class auto_addrinfo
    : private netio::detail::noncopyable
  {
  public:
    explicit auto_addrinfo(netio::detail::addrinfo_type* ai)
      : ai_(ai)
    {
    }

    ~auto_addrinfo()
    {
      if (ai_)
        socket_ops::freeaddrinfo(ai_);
    }

    operator netio::detail::addrinfo_type*()
    {
      return ai_;
    }

  private:
    netio::detail::addrinfo_type* ai_;
  };
#endif // !defined(NETIO_WINDOWS_RUNTIME)

  // Private thread pool used for performing asynchronous host resolution.
  resolver_thread_pool& thread_pool_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/resolver_service_base.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // NETIO_DETAIL_RESOLVER_SERVICE_BASE_HPP
