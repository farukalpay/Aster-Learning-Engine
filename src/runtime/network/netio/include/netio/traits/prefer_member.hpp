#ifndef NETIO_TRAITS_PREFER_MEMBER_HPP
#define NETIO_TRAITS_PREFER_MEMBER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#if defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)
# define NETIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT 1
#endif // defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace traits {

template <typename T, typename Property, typename = void>
struct prefer_member_default;

template <typename T, typename Property, typename = void>
struct prefer_member;

} // namespace traits
namespace detail {

struct no_prefer_member
{
  static constexpr bool is_valid = false;
  static constexpr bool is_noexcept = false;
};

#if defined(NETIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT)

template <typename T, typename Property, typename = void>
struct prefer_member_trait : no_prefer_member
{
};

template <typename T, typename Property>
struct prefer_member_trait<T, Property,
  void_t<
    decltype(declval<T>().prefer(declval<Property>()))
  >>
{
  static constexpr bool is_valid = true;

  using result_type = decltype(
    declval<T>().prefer(declval<Property>()));

  static constexpr bool is_noexcept =
    noexcept(declval<T>().prefer(declval<Property>()));
};

#else // defined(NETIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT)

template <typename T, typename Property, typename = void>
struct prefer_member_trait :
  conditional_t<
    is_same<T, decay_t<T>>::value
      && is_same<Property, decay_t<Property>>::value,
    no_prefer_member,
    traits::prefer_member<
      decay_t<T>,
      decay_t<Property>>
  >
{
};

#endif // defined(NETIO_HAS_DEDUCED_PREFER_MEMBER_TRAIT)

} // namespace detail
namespace traits {

template <typename T, typename Property, typename>
struct prefer_member_default :
  detail::prefer_member_trait<T, Property>
{
};

template <typename T, typename Property, typename>
struct prefer_member :
  prefer_member_default<T, Property>
{
};

} // namespace traits
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_TRAITS_PREFER_MEMBER_HPP
