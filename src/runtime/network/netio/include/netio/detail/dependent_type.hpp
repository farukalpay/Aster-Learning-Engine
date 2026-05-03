#ifndef NETIO_DETAIL_DEPENDENT_TYPE_HPP
#define NETIO_DETAIL_DEPENDENT_TYPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename DependsOn, typename T>
struct dependent_type
{
  typedef T type;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_DEPENDENT_TYPE_HPP
