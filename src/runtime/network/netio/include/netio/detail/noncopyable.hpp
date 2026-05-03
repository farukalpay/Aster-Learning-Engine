#ifndef NETIO_DETAIL_NONCOPYABLE_HPP
#define NETIO_DETAIL_NONCOPYABLE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class noncopyable
{
protected:
  noncopyable() {}
  ~noncopyable() {}
private:
  noncopyable(const noncopyable&);
  const noncopyable& operator=(const noncopyable&);
};

} // namespace detail

using netio::detail::noncopyable;

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_NONCOPYABLE_HPP
