#ifndef NETIO_DETAIL_NULL_GLOBAL_HPP
#define NETIO_DETAIL_NULL_GLOBAL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
struct null_global_impl
{
  null_global_impl()
    : ptr_(0)
  {
  }

  // Destructor automatically cleans up the global.
  ~null_global_impl()
  {
    delete ptr_;
  }

  static null_global_impl instance_;
  T* ptr_;
};

template <typename T>
null_global_impl<T> null_global_impl<T>::instance_;

template <typename T>
T& null_global()
{
  if (null_global_impl<T>::instance_.ptr_ == 0)
    null_global_impl<T>::instance_.ptr_ = new T;
  return *null_global_impl<T>::instance_.ptr_;
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_NULL_GLOBAL_HPP
