#ifndef NETIO_IMPL_ANY_COMPLETION_EXECUTOR_IPP
#define NETIO_IMPL_ANY_COMPLETION_EXECUTOR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_USE_TS_EXECUTOR_AS_DEFAULT)

#include "netio/any_completion_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

any_completion_executor::any_completion_executor() noexcept
  : base_type()
{
}

any_completion_executor::any_completion_executor(nullptr_t) noexcept
  : base_type(nullptr_t())
{
}

any_completion_executor::any_completion_executor(
    const any_completion_executor& e) noexcept
  : base_type(static_cast<const base_type&>(e))
{
}

any_completion_executor::any_completion_executor(std::nothrow_t,
    const any_completion_executor& e) noexcept
  : base_type(static_cast<const base_type&>(e))
{
}

any_completion_executor::any_completion_executor(
    any_completion_executor&& e) noexcept
  : base_type(static_cast<base_type&&>(e))
{
}

any_completion_executor::any_completion_executor(std::nothrow_t,
    any_completion_executor&& e) noexcept
  : base_type(static_cast<base_type&&>(e))
{
}

any_completion_executor& any_completion_executor::operator=(
    const any_completion_executor& e) noexcept
{
  base_type::operator=(static_cast<const base_type&>(e));
  return *this;
}

any_completion_executor& any_completion_executor::operator=(
    any_completion_executor&& e) noexcept
{
  base_type::operator=(static_cast<base_type&&>(e));
  return *this;
}

any_completion_executor& any_completion_executor::operator=(nullptr_t)
{
  base_type::operator=(nullptr_t());
  return *this;
}

any_completion_executor::~any_completion_executor()
{
}

void any_completion_executor::swap(
    any_completion_executor& other) noexcept
{
  static_cast<base_type&>(*this).swap(static_cast<base_type&>(other));
}

template <>
any_completion_executor any_completion_executor::prefer(
    const execution::outstanding_work_t::tracked_t& p, int) const
{
  return static_cast<const base_type&>(*this).prefer(p);
}

template <>
any_completion_executor any_completion_executor::prefer(
    const execution::outstanding_work_t::untracked_t& p, int) const
{
  return static_cast<const base_type&>(*this).prefer(p);
}

template <>
any_completion_executor any_completion_executor::prefer(
    const execution::relationship_t::fork_t& p, int) const
{
  return static_cast<const base_type&>(*this).prefer(p);
}

template <>
any_completion_executor any_completion_executor::prefer(
    const execution::relationship_t::continuation_t& p, int) const
{
  return static_cast<const base_type&>(*this).prefer(p);
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_USE_TS_EXECUTOR_AS_DEFAULT)

#endif // NETIO_IMPL_ANY_COMPLETION_EXECUTOR_IPP
