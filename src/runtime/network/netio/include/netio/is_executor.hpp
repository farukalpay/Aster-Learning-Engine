#ifndef NETIO_IS_EXECUTOR_HPP
#define NETIO_IS_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/is_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// The is_executor trait detects whether a type T meets the Executor type
/// requirements.
/**
 * Class template @c is_executor is a UnaryTypeTrait that is derived from @c
 * true_type if the type @c T meets the syntactic requirements for Executor,
 * otherwise @c false_type.
 */
template <typename T>
struct is_executor
#if defined(GENERATING_DOCUMENTATION)
  : integral_constant<bool, automatically_determined>
#else // defined(GENERATING_DOCUMENTATION)
  : netio::detail::is_executor<T>
#endif // defined(GENERATING_DOCUMENTATION)
{
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IS_EXECUTOR_HPP
