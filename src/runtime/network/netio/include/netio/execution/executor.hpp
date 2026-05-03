#ifndef NETIO_EXECUTION_EXECUTOR_HPP
#define NETIO_EXECUTION_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/execution/invocable_archetype.hpp"
#include "netio/traits/equality_comparable.hpp"
#include "netio/traits/execute_member.hpp"

#if defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT) \
  && defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)
# define NETIO_HAS_DEDUCED_EXECUTION_IS_EXECUTOR_TRAIT 1
#endif // defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)
       //   && defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace execution {
namespace detail {

template <typename T, typename F,
    typename = void, typename = void, typename = void, typename = void,
    typename = void, typename = void, typename = void, typename = void>
struct is_executor_of_impl : false_type
{
};

template <typename T, typename F>
struct is_executor_of_impl<T, F,
  enable_if_t<
    traits::execute_member<add_const_t<T>, F>::is_valid
  >,
  void_t<
    result_of_t<decay_t<F>&()>
  >,
  enable_if_t<
    is_constructible<decay_t<F>, F>::value
  >,
  enable_if_t<
    is_move_constructible<decay_t<F>>::value
  >,
  enable_if_t<
    is_nothrow_copy_constructible<T>::value
  >,
  enable_if_t<
    is_nothrow_destructible<T>::value
  >,
  enable_if_t<
    traits::equality_comparable<T>::is_valid
  >,
  enable_if_t<
    traits::equality_comparable<T>::is_noexcept
  >> : true_type
{
};

} // namespace detail

/// The is_executor trait detects whether a type T satisfies the
/// execution::executor concept.
/**
 * Class template @c is_executor is a UnaryTypeTrait that is derived from @c
 * true_type if the type @c T meets the concept definition for an executor,
 * otherwise @c false_type.
 */
template <typename T>
struct is_executor :
#if defined(GENERATING_DOCUMENTATION)
  integral_constant<bool, automatically_determined>
#else // defined(GENERATING_DOCUMENTATION)
  detail::is_executor_of_impl<T, invocable_archetype>
#endif // defined(GENERATING_DOCUMENTATION)
{
};

#if defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T>
constexpr const bool is_executor_v = is_executor<T>::value;

#endif // defined(NETIO_HAS_VARIABLE_TEMPLATES)

#if defined(NETIO_HAS_CONCEPTS)

template <typename T>
NETIO_CONCEPT executor = is_executor<T>::value;

#define NETIO_EXECUTION_EXECUTOR ::netio::execution::executor

#else // defined(NETIO_HAS_CONCEPTS)

#define NETIO_EXECUTION_EXECUTOR typename

#endif // defined(NETIO_HAS_CONCEPTS)

} // namespace execution
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXECUTION_EXECUTOR_HPP
