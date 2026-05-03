#ifndef NETIO_DETAIL_KEYWORD_TSS_PTR_HPP
#define NETIO_DETAIL_KEYWORD_TSS_PTR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_THREAD_KEYWORD_EXTENSION)

#include "netio/detail/noncopyable.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
class keyword_tss_ptr
  : private noncopyable
{
public:
  // Constructor.
  keyword_tss_ptr()
  {
  }

  // Destructor.
  ~keyword_tss_ptr()
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
  static NETIO_THREAD_KEYWORD T* value_;
};

template <typename T>
NETIO_THREAD_KEYWORD T* keyword_tss_ptr<T>::value_;

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_HAS_THREAD_KEYWORD_EXTENSION)

#endif // NETIO_DETAIL_KEYWORD_TSS_PTR_HPP
