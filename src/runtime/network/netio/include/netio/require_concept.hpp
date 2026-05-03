#ifndef NETIO_REQUIRE_CONCEPT_HPP
#define NETIO_REQUIRE_CONCEPT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/is_applicable_property.hpp"
#include "netio/traits/require_concept_member.hpp"
#include "netio/traits/require_concept_free.hpp"
#include "netio/traits/static_require_concept.hpp"

#include "netio/detail/push_options.hpp"

#if defined(GENERATING_DOCUMENTATION)

namespace netio {

/// A customisation point that applies a concept-enforcing property to an
/// object.
/**
 * The name <tt>require_concept</tt> denotes a customization point object. The
 * expression <tt>netio::require_concept(E, P)</tt> for some
 * subexpressions <tt>E</tt> and <tt>P</tt> (with types <tt>T =
 * decay_t<decltype(E)></tt> and <tt>Prop = decay_t<decltype(P)></tt>) is
 * expression-equivalent to:
 *
 * @li If <tt>is_applicable_property_v<T, Prop> &&
 *   Prop::is_requirable_concept</tt> is not a well-formed constant expression
 *   with value <tt>true</tt>, <tt>netio::require_concept(E, P)</tt> is
 *   ill-formed.
 *
 * @li Otherwise, <tt>E</tt> if the expression <tt>Prop::template
 *   static_query_v<T> == Prop::value()</tt> is a well-formed constant
 *   expression with value <tt>true</tt>.
 *
 * @li Otherwise, <tt>(E).require_concept(P)</tt> if the expression
 *   <tt>(E).require_concept(P)</tt> is well-formed.
 *
 * @li Otherwise, <tt>require_concept(E, P)</tt> if the expression
 *   <tt>require_concept(E, P)</tt> is a valid expression with overload
 *   resolution performed in a context that does not include the declaration
 *   of the <tt>require_concept</tt> customization point object.
 *
 * @li Otherwise, <tt>netio::require_concept(E, P)</tt> is ill-formed.
 */
inline constexpr unspecified require_concept = unspecified;

/// A type trait that determines whether a @c require_concept expression is
/// well-formed.
/**
 * Class template @c can_require_concept is a trait that is derived from
 * @c true_type if the expression
 * <tt>netio::require_concept(std::declval<T>(),
 * std::declval<Property>())</tt> is well formed; otherwise @c false_type.
 */
template <typename T, typename Property>
struct can_require_concept :
  integral_constant<bool, automatically_determined>
{
};

/// A type trait that determines whether a @c require_concept expression will
/// not throw.
/**
 * Class template @c is_nothrow_require_concept is a trait that is derived from
 * @c true_type if the expression
 * <tt>netio::require_concept(std::declval<T>(),
 * std::declval<Property>())</tt> is @c noexcept; otherwise @c false_type.
 */
template <typename T, typename Property>
struct is_nothrow_require_concept :
  integral_constant<bool, automatically_determined>
{
};

/// A type trait that determines the result type of a @c require_concept
/// expression.
/**
 * Class template @c require_concept_result is a trait that determines the
 * result type of the expression
 * <tt>netio::require_concept(std::declval<T>(),
 * std::declval<Property>())</tt>.
 */
template <typename T, typename Property>
struct require_concept_result
{
  /// The result of the @c require_concept expression.
  typedef automatically_determined type;
};

} // namespace netio

#else // defined(GENERATING_DOCUMENTATION)

namespace netio_require_concept_fn {

using netio::conditional_t;
using netio::decay_t;
using netio::declval;
using netio::enable_if_t;
using netio::is_applicable_property;
using netio::traits::require_concept_free;
using netio::traits::require_concept_member;
using netio::traits::static_require_concept;

void require_concept();

enum overload_type
{
  identity,
  call_member,
  call_free,
  ill_formed
};

template <typename Impl, typename T, typename Properties, typename = void,
    typename = void, typename = void, typename = void, typename = void>
struct call_traits
{
  static constexpr overload_type overload = ill_formed;
  static constexpr bool is_noexcept = false;
  typedef void result_type;
};

template <typename Impl, typename T, typename Property>
struct call_traits<Impl, T, void(Property),
  enable_if_t<
    is_applicable_property<
      decay_t<T>,
      decay_t<Property>
    >::value
  >,
  enable_if_t<
    decay_t<Property>::is_requirable_concept
  >,
  enable_if_t<
    static_require_concept<T, Property>::is_valid
  >>
{
  static constexpr overload_type overload = identity;
  static constexpr bool is_noexcept = true;
  typedef T&& result_type;
};

template <typename Impl, typename T, typename Property>
struct call_traits<Impl, T, void(Property),
  enable_if_t<
    is_applicable_property<
      decay_t<T>,
      decay_t<Property>
    >::value
  >,
  enable_if_t<
    decay_t<Property>::is_requirable_concept
  >,
  enable_if_t<
    !static_require_concept<T, Property>::is_valid
  >,
  enable_if_t<
    require_concept_member<
      typename Impl::template proxy<T>::type,
      Property
    >::is_valid
  >> :
  require_concept_member<
    typename Impl::template proxy<T>::type,
    Property
  >
{
  static constexpr overload_type overload = call_member;
};

template <typename Impl, typename T, typename Property>
struct call_traits<Impl, T, void(Property),
  enable_if_t<
    is_applicable_property<
      decay_t<T>,
      decay_t<Property>
    >::value
  >,
  enable_if_t<
    decay_t<Property>::is_requirable_concept
  >,
  enable_if_t<
    !static_require_concept<T, Property>::is_valid
  >,
  enable_if_t<
    !require_concept_member<
      typename Impl::template proxy<T>::type,
      Property
    >::is_valid
  >,
  enable_if_t<
    require_concept_free<T, Property>::is_valid
  >> :
  require_concept_free<T, Property>
{
  static constexpr overload_type overload = call_free;
};

struct impl
{
  template <typename T>
  struct proxy
  {
#if defined(NETIO_HAS_DEDUCED_REQUIRE_CONCEPT_MEMBER_TRAIT)
    struct type
    {
      template <typename P>
      auto require_concept(P&& p)
        noexcept(
          noexcept(
            declval<conditional_t<true, T, P>>().require_concept(
              static_cast<P&&>(p))
          )
        )
        -> decltype(
          declval<conditional_t<true, T, P>>().require_concept(
            static_cast<P&&>(p))
        );
    };
#else // defined(NETIO_HAS_DEDUCED_REQUIRE_CONCEPT_MEMBER_TRAIT)
    typedef T type;
#endif // defined(NETIO_HAS_DEDUCED_REQUIRE_CONCEPT_MEMBER_TRAIT)
  };

  template <typename T, typename Property>
  NETIO_NODISCARD constexpr enable_if_t<
    call_traits<impl, T, void(Property)>::overload == identity,
    typename call_traits<impl, T, void(Property)>::result_type
  >
  operator()(T&& t, Property&&) const
    noexcept(call_traits<impl, T, void(Property)>::is_noexcept)
  {
    return static_cast<T&&>(t);
  }

  template <typename T, typename Property>
  NETIO_NODISCARD constexpr enable_if_t<
    call_traits<impl, T, void(Property)>::overload == call_member,
    typename call_traits<impl, T, void(Property)>::result_type
  >
  operator()(T&& t, Property&& p) const
    noexcept(call_traits<impl, T, void(Property)>::is_noexcept)
  {
    return static_cast<T&&>(t).require_concept(static_cast<Property&&>(p));
  }

  template <typename T, typename Property>
  NETIO_NODISCARD constexpr enable_if_t<
    call_traits<impl, T, void(Property)>::overload == call_free,
    typename call_traits<impl, T, void(Property)>::result_type
  >
  operator()(T&& t, Property&& p) const
    noexcept(call_traits<impl, T, void(Property)>::is_noexcept)
  {
    return require_concept(static_cast<T&&>(t), static_cast<Property&&>(p));
  }
};

template <typename T = impl>
struct static_instance
{
  static const T instance;
};

template <typename T>
const T static_instance<T>::instance = {};

} // namespace netio_require_concept_fn
namespace netio {
namespace {

static constexpr const netio_require_concept_fn::impl&
  require_concept = netio_require_concept_fn::static_instance<>::instance;

} // namespace

typedef netio_require_concept_fn::impl require_concept_t;

template <typename T, typename Property>
struct can_require_concept :
  integral_constant<bool,
    netio_require_concept_fn::call_traits<
      require_concept_t, T, void(Property)>::overload !=
        netio_require_concept_fn::ill_formed>
{
};

#if defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
constexpr bool can_require_concept_v = can_require_concept<T, Property>::value;

#endif // defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
struct is_nothrow_require_concept :
  integral_constant<bool,
    netio_require_concept_fn::call_traits<
      require_concept_t, T, void(Property)>::is_noexcept>
{
};

#if defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
constexpr bool is_nothrow_require_concept_v
  = is_nothrow_require_concept<T, Property>::value;

#endif // defined(NETIO_HAS_VARIABLE_TEMPLATES)

template <typename T, typename Property>
struct require_concept_result
{
  typedef typename netio_require_concept_fn::call_traits<
      require_concept_t, T, void(Property)>::result_type type;
};

template <typename T, typename Property>
using require_concept_result_t =
  typename require_concept_result<T, Property>::type;

} // namespace netio

#endif // defined(GENERATING_DOCUMENTATION)

#include "netio/detail/pop_options.hpp"

#endif // NETIO_REQUIRE_CONCEPT_HPP
