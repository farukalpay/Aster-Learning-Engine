#ifndef NETIO_EXPERIMENTAL_DETAIL_HAS_SIGNATURE_HPP
#define NETIO_EXPERIMENTAL_DETAIL_HAS_SIGNATURE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {
namespace detail {

template <typename S, typename... Signatures>
struct has_signature;

template <typename S, typename... Signatures>
struct has_signature;

template <typename S>
struct has_signature<S> : false_type
{
};

template <typename S, typename... Signatures>
struct has_signature<S, S, Signatures...> : true_type
{
};

template <typename S, typename Head, typename... Tail>
struct has_signature<S, Head, Tail...> : has_signature<S, Tail...>
{
};

} // namespace detail
} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_DETAIL_HAS_SIGNATURE_HPP
