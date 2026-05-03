#ifndef NETIO_IP_IMPL_ADDRESS_HPP
#define NETIO_IP_IMPL_ADDRESS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if !defined(NETIO_NO_IOSTREAM)

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

template <typename Elem, typename Traits>
std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const address& addr)
{
  return os << addr.to_string().c_str();
}

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_NO_IOSTREAM)

#endif // NETIO_IP_IMPL_ADDRESS_HPP
