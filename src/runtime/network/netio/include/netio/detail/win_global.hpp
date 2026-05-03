#ifndef NETIO_DETAIL_WIN_GLOBAL_HPP
#define NETIO_DETAIL_WIN_GLOBAL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/static_mutex.hpp"
#include "netio/detail/tss_ptr.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
struct win_global_impl
{
  // Destructor automatically cleans up the global.
  ~win_global_impl()
  {
    delete ptr_;
  }

  static win_global_impl instance_;
  static static_mutex mutex_;
  T* ptr_;
  static tss_ptr<T> tss_ptr_;
};

template <typename T>
win_global_impl<T> win_global_impl<T>::instance_ = { 0 };

template <typename T>
static_mutex win_global_impl<T>::mutex_ = NETIO_STATIC_MUTEX_INIT;

template <typename T>
tss_ptr<T> win_global_impl<T>::tss_ptr_;

template <typename T>
T& win_global()
{
  if (static_cast<T*>(win_global_impl<T>::tss_ptr_) == 0)
  {
    win_global_impl<T>::mutex_.init();
    static_mutex::scoped_lock lock(win_global_impl<T>::mutex_);
    if (win_global_impl<T>::instance_.ptr_ == 0)
      win_global_impl<T>::instance_.ptr_ = new T;
    win_global_impl<T>::tss_ptr_ = win_global_impl<T>::instance_.ptr_;
  }

  return *win_global_impl<T>::tss_ptr_;
}

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_WIN_GLOBAL_HPP
