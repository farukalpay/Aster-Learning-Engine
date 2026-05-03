#ifndef NETIO_DEFAULT_COMPLETION_TOKEN_HPP
#define NETIO_DEFAULT_COMPLETION_TOKEN_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

class deferred_t;

namespace detail {

template <typename T, typename = void>
struct default_completion_token_impl
{
  typedef deferred_t type;
};

template <typename T>
struct default_completion_token_impl<T,
    void_t<typename T::default_completion_token_type>
  >
{
  typedef typename T::default_completion_token_type type;
};

} // namespace detail

#if defined(GENERATING_DOCUMENTATION)

/// Traits type used to determine the default completion token type associated
/// with a type (such as an executor).
/**
 * A program may specialise this traits type if the @c T template parameter in
 * the specialisation is a user-defined type.
 *
 * Specialisations of this trait may provide a nested typedef @c type, which is
 * a default-constructible completion token type.
 *
 * If not otherwise specialised, the default completion token type is
 * netio::deferred_t.
 */
template <typename T>
struct default_completion_token
{
  /// If @c T has a nested type @c default_completion_token_type,
  /// <tt>T::default_completion_token_type</tt>. Otherwise the typedef @c type
  /// is netio::deferred_t.
  typedef see_below type;
};
#else
template <typename T>
struct default_completion_token
  : detail::default_completion_token_impl<T>
{
};
#endif

template <typename T>
using default_completion_token_t = typename default_completion_token<T>::type;

#define NETIO_DEFAULT_COMPLETION_TOKEN_TYPE(e) \
  = typename ::netio::default_completion_token<e>::type
#define NETIO_DEFAULT_COMPLETION_TOKEN(e) \
  = typename ::netio::default_completion_token<e>::type()

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/deferred.hpp"

#endif // NETIO_DEFAULT_COMPLETION_TOKEN_HPP
