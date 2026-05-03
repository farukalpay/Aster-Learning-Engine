#ifndef NETIO_DETAIL_NULL_TSS_PTR_HPP
#define NETIO_DETAIL_NULL_TSS_PTR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)

#include "netio/detail/noncopyable.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
class null_tss_ptr
  : private noncopyable
{
public:
  // Constructor.
  null_tss_ptr()
    : value_(0)
  {
  }

  // Destructor.
  ~null_tss_ptr()
  {
  }

  // Get the value.
  operator T*() const
  {
    return value_;
  }

  // Set the value.
  void operator=(T* value)
  {
    value_ = value;
  }

private:
  T* value_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_HAS_THREADS)

#endif // NETIO_DETAIL_NULL_TSS_PTR_HPP
