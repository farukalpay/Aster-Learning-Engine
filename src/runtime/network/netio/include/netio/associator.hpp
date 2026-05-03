#ifndef NETIO_ASSOCIATOR_HPP
#define NETIO_ASSOCIATOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// Used to generically specialise associators for a type.
template <template <typename, typename> class Associator,
    typename T, typename DefaultCandidate, typename _ = void>
struct associator
{
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_ASSOCIATOR_HPP
