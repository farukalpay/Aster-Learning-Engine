#ifndef NETIO_USES_EXECUTOR_HPP
#define NETIO_USES_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// A special type, similar to std::nothrow_t, used to disambiguate
/// constructors that accept executor arguments.
/**
 * The executor_arg_t struct is an empty structure type used as a unique type
 * to disambiguate constructor and function overloading. Specifically, some
 * types have constructors with executor_arg_t as the first argument,
 * immediately followed by an argument of a type that satisfies the Executor
 * type requirements.
 */
struct executor_arg_t
{
  /// Constructor.
  constexpr executor_arg_t() noexcept
  {
  }
};

/// A special value, similar to std::nothrow, used to disambiguate constructors
/// that accept executor arguments.
/**
 * See netio::executor_arg_t and netio::uses_executor
 * for more information.
 */
NETIO_INLINE_VARIABLE constexpr executor_arg_t executor_arg;

/// The uses_executor trait detects whether a type T has an associated executor
/// that is convertible from type Executor.
/**
 * Meets the BinaryTypeTrait requirements. The Netio library provides a
 * definition that is derived from false_type. A program may specialize this
 * template to derive from true_type for a user-defined type T that can be
 * constructed with an executor, where the first argument of a constructor has
 * type executor_arg_t and the second argument is convertible from type
 * Executor.
 */
template <typename T, typename Executor>
struct uses_executor : false_type {};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_USES_EXECUTOR_HPP
