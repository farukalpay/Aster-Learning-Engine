#ifndef NETIO_IP_RESOLVER_QUERY_BASE_HPP
#define NETIO_IP_RESOLVER_QUERY_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/ip/resolver_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

/// The resolver_query_base class is used as a base for the
/// basic_resolver_query class templates to provide a common place to define
/// the flag constants.
class resolver_query_base : public resolver_base
{
protected:
  /// Protected destructor to prevent deletion through this type.
  ~resolver_query_base()
  {
  }
};

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IP_RESOLVER_QUERY_BASE_HPP
