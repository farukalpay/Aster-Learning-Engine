#ifndef NETIO_DETAIL_IMPL_RESOLVER_SERVICE_BASE_IPP
#define NETIO_DETAIL_IMPL_RESOLVER_SERVICE_BASE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/resolver_service_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

resolver_service_base::resolver_service_base(execution_context& context)
  : thread_pool_(netio::use_service<resolver_thread_pool>(context))
{
}

resolver_service_base::~resolver_service_base()
{
}

void resolver_service_base::construct(
    resolver_service_base::implementation_type& impl)
{
  impl.reset(static_cast<void*>(0), socket_ops::noop_deleter());
}

void resolver_service_base::destroy(
    resolver_service_base::implementation_type& impl)
{
  NETIO_HANDLER_OPERATION((thread_pool_.context(),
        "resolver", &impl, 0, "cancel"));

  impl.reset();
}

void resolver_service_base::move_construct(implementation_type& impl,
    implementation_type& other_impl)
{
  impl = static_cast<implementation_type&&>(other_impl);
}

void resolver_service_base::move_assign(implementation_type& impl,
    resolver_service_base&, implementation_type& other_impl)
{
  destroy(impl);
  impl = static_cast<implementation_type&&>(other_impl);
}

void resolver_service_base::cancel(
    resolver_service_base::implementation_type& impl)
{
  NETIO_HANDLER_OPERATION((thread_pool_.context(),
        "resolver", &impl, 0, "cancel"));

  impl.reset(static_cast<void*>(0), socket_ops::noop_deleter());
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_IMPL_RESOLVER_SERVICE_BASE_IPP
