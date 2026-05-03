#ifndef NETIO_TRAITS_QUERY_STATIC_CONSTEXPR_MEMBER_HPP
#define NETIO_TRAITS_QUERY_STATIC_CONSTEXPR_MEMBER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#if defined(NETIO_HAS_CONSTANT_EXPRESSION_SFINAE) \
  && defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)
# define NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT 1
#endif // defined(NETIO_HAS_CONSTANT_EXPRESSION_SFINAE)
       //   && defined(NETIO_HAS_WORKING_EXPRESSION_SFINAE)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace traits {

template <typename T, typename Property, typename = void>
struct query_static_constexpr_member_default;

template <typename T, typename Property, typename = void>
struct query_static_constexpr_member;

} // namespace traits
namespace detail {

struct no_query_static_constexpr_member
{
  static constexpr bool is_valid = false;
};

template <typename T, typename Property, typename = void>
struct query_static_constexpr_member_trait :
  conditional_t<
    is_same<T, decay_t<T>>::value
      && is_same<Property, decay_t<Property>>::value,
    no_query_static_constexpr_member,
    traits::query_static_constexpr_member<
      decay_t<T>,
      decay_t<Property>>
  >
{
};

#if defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

template <typename T, typename Property>
struct query_static_constexpr_member_trait<T, Property,
  enable_if_t<
    (static_cast<void>(T::query(Property{})), true)
  >>
{
  static constexpr bool is_valid = true;

  using result_type = decltype(T::query(Property{}));

  static constexpr bool is_noexcept = noexcept(T::query(Property{}));

  static constexpr result_type value() noexcept(is_noexcept)
  {
    return T::query(Property{});
  }
};

#endif // defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

} // namespace detail
namespace traits {

template <typename T, typename Property, typename>
struct query_static_constexpr_member_default :
  detail::query_static_constexpr_member_trait<T, Property>
{
};

template <typename T, typename Property, typename>
struct query_static_constexpr_member :
  query_static_constexpr_member_default<T, Property>
{
};

} // namespace traits
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_TRAITS_QUERY_STATIC_CONSTEXPR_MEMBER_HPP
