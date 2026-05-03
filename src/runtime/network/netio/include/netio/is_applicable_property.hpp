#ifndef NETIO_IS_APPLICABLE_PROPERTY_HPP
#define NETIO_IS_APPLICABLE_PROPERTY_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

namespace netio {
namespace detail {

template <typename T, typename Property, typename = void>
struct is_applicable_property_trait : false_type
{
};

#if defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
struct is_applicable_property_trait<T, Property,
  void_t<
    enable_if_t<
      !!Property::template is_applicable_property_v<T>
    >
  >> : true_type
{
};

#endif // defined(NETIO_HAS_VARIABLE_TEMPLATES)

} // namespace detail

template <typename T, typename Property, typename = void>
struct is_applicable_property :
  detail::is_applicable_property_trait<T, Property>
{
};

#if defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
constexpr const bool is_applicable_property_v
  = is_applicable_property<T, Property>::value;

#endif // defined(NETIO_HAS_VARIABLE_TEMPLATES)

} // namespace netio

#endif // NETIO_IS_APPLICABLE_PROPERTY_HPP
