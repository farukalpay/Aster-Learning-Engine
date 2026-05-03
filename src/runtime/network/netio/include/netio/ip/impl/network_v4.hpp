#ifndef NETIO_IP_IMPL_NETWORK_V4_HPP
#define NETIO_IP_IMPL_NETWORK_V4_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if !defined(NETIO_NO_IOSTREAM)

#include "netio/detail/throw_error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ip {

template <typename Elem, typename Traits>
std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const network_v4& addr)
{
  netio::error_code ec;
  std::string s = addr.to_string(ec);
  if (ec)
  {
    if (os.exceptions() & std::basic_ostream<Elem, Traits>::failbit)
      netio::detail::throw_error(ec);
    else
      os.setstate(std::basic_ostream<Elem, Traits>::failbit);
  }
  else
    for (std::string::iterator i = s.begin(); i != s.end(); ++i)
      os << os.widen(*i);
  return os;
}

} // namespace ip
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_NO_IOSTREAM)

#endif // NETIO_IP_IMPL_NETWORK_V4_HPP
