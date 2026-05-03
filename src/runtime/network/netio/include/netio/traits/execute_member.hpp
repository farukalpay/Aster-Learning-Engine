#ifndef NETIO_TRAITS_EXECUTE_MEMBER_HPP
#define NETIO_TRAITS_EXECUTE_MEMBER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#if defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)
# define NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT 1
#endif // defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace traits {

template <typename T, typename F, typename = void>
struct execute_member_default;

template <typename T, typename F, typename = void>
struct execute_member;

} // namespace traits
namespace detail {

struct no_execute_member
{
  static constexpr bool is_valid = false;
  static constexpr bool is_noexcept = false;
};

#if defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename T, typename F, typename = void>
struct execute_member_trait : no_execute_member
{
};

template <typename T, typename F>
struct execute_member_trait<T, F,
  void_t<
    decltype(declval<T>().execute(declval<F>()))
  >>
{
  static constexpr bool is_valid = true;

  using result_type = decltype(
    declval<T>().execute(declval<F>()));

  static constexpr bool is_noexcept =
    noexcept(declval<T>().execute(declval<F>()));
};

#else // defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename T, typename F, typename = void>
struct execute_member_trait :
  conditional_t<
    is_same<T, decay_t<T>>::value
      && is_same<F, decay_t<F>>::value,
    no_execute_member,
    traits::execute_member<
      decay_t<T>,
      decay_t<F>>
  >
{
};

#endif // defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

} // namespace detail
namespace traits {

template <typename T, typename F, typename>
struct execute_member_default :
  detail::execute_member_trait<T, F>
{
};

template <typename T, typename F, typename>
struct execute_member :
  execute_member_default<T, F>
{
};

} // namespace traits
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_TRAITS_EXECUTE_MEMBER_HPP
