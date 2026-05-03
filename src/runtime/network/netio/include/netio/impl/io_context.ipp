#ifndef NETIO_IMPL_IO_CONTEXT_IPP
#define NETIO_IMPL_IO_CONTEXT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/config.hpp"
#include "netio/io_context.hpp"
#include "netio/detail/concurrency_hint.hpp"
#include "netio/detail/limits.hpp"
#include "netio/detail/service_registry.hpp"
#include "netio/detail/throw_error.hpp"

#if defined(NETIO_HAS_IOCP)
# include "netio/detail/win_iocp_io_context.hpp"
#else
# include "netio/detail/scheduler.hpp"
#endif

#include "netio/detail/push_options.hpp"

namespace netio {

io_context::io_context()
  : execution_context(config_from_concurrency_hint()),
    impl_(netio::make_service<impl_type>(*this, false))
{
}

io_context::io_context(int concurrency_hint)
  : execution_context(config_from_concurrency_hint(concurrency_hint)),
    impl_(netio::make_service<impl_type>(*this, false))
{
}

io_context::io_context(const execution_context::service_maker& initial_services)
  : execution_context(initial_services),
    impl_(netio::make_service<impl_type>(*this, false))
{
}

io_context::~io_context()
{
  shutdown();
}

io_context::count_type io_context::run()
{
  netio::error_code ec;
  count_type s = impl_.run(ec);
  netio::detail::throw_error(ec);
  return s;
}

io_context::count_type io_context::run_one()
{
  netio::error_code ec;
  count_type s = impl_.run_one(ec);
  netio::detail::throw_error(ec);
  return s;
}

io_context::count_type io_context::poll()
{
  netio::error_code ec;
  count_type s = impl_.poll(ec);
  netio::detail::throw_error(ec);
  return s;
}

io_context::count_type io_context::poll_one()
{
  netio::error_code ec;
  count_type s = impl_.poll_one(ec);
  netio::detail::throw_error(ec);
  return s;
}

void io_context::stop()
{
  impl_.stop();
}

bool io_context::stopped() const
{
  return impl_.stopped();
}

void io_context::restart()
{
  impl_.restart();
}

io_context::service::service(netio::io_context& owner)
  : execution_context::service(owner)
{
}

io_context::service::~service()
{
}

void io_context::service::shutdown()
{
}

void io_context::service::notify_fork(io_context::fork_event)
{
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IMPL_IO_CONTEXT_IPP
