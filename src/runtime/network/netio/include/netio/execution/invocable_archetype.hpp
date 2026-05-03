#ifndef NETIO_EXECUTION_INVOCABLE_ARCHETYPE_HPP
#define NETIO_EXECUTION_INVOCABLE_ARCHETYPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace execution {

/// An archetypal function object used for determining adherence to the
/// execution::executor concept.
struct invocable_archetype
{
  /// Function call operator.
  template <typename... Args>
  void operator()(Args&&...)
  {
  }
};

} // namespace execution
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXECUTION_INVOCABLE_ARCHETYPE_HPP

