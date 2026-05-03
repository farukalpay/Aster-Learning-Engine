#ifndef NETIO_IS_CONTIGUOUS_ITERATOR_HPP
#define NETIO_IS_CONTIGUOUS_ITERATOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <iterator>
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// The is_contiguous_iterator class is a traits class that may be used to
/// determine whether a type is a contiguous iterator.
template <typename T>
struct is_contiguous_iterator :
#if defined(NETIO_HAS_STD_CONCEPTS) \
  || defined(GENERATING_DOCUMENTATION)
  integral_constant<bool, std::contiguous_iterator<T>>
#else // defined(NETIO_HAS_STD_CONCEPTS)
      //   || defined(GENERATING_DOCUMENTATION)
  is_pointer<T>
#endif // defined(NETIO_HAS_STD_CONCEPTS)
       //   || defined(GENERATING_DOCUMENTATION)
{
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IS_CONTIGUOUS_ITERATOR_HPP
