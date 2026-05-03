#ifndef NETIO_DETAIL_INITIATION_BASE_HPP
#define NETIO_DETAIL_INITIATION_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Initiation, typename = void>
class initiation_base : public Initiation
{
public:
  template <typename I>
  explicit initiation_base(I&& initiation)
    : Initiation(static_cast<I&&>(initiation))
  {
  }
};

template <typename Initiation>
class initiation_base<Initiation, enable_if_t<!is_class<Initiation>::value>>
{
public:
  template <typename I>
  explicit initiation_base(I&& initiation)
    : initiation_(static_cast<I&&>(initiation))
  {
  }

  template <typename... Args>
  void operator()(Args&&... args) const
  {
    initiation_(static_cast<Args&&>(args)...);
  }

private:
  Initiation initiation_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_INITIATION_BASE_HPP
