#ifndef NETIO_DETAIL_TSS_PTR_HPP
#define NETIO_DETAIL_TSS_PTR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
# include "netio/detail/null_tss_ptr.hpp"
#elif defined(NETIO_HAS_THREAD_KEYWORD_EXTENSION)
# include "netio/detail/keyword_tss_ptr.hpp"
#elif defined(NETIO_WINDOWS)
# include "netio/detail/win_tss_ptr.hpp"
#elif defined(NETIO_HAS_PTHREADS)
# include "netio/detail/posix_tss_ptr.hpp"
#else
# error Only Windows and POSIX are supported!
#endif

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T>
class tss_ptr
#if !defined(NETIO_HAS_THREADS)
  : public null_tss_ptr<T>
#elif defined(NETIO_HAS_THREAD_KEYWORD_EXTENSION)
  : public keyword_tss_ptr<T>
#elif defined(NETIO_WINDOWS)
  : public win_tss_ptr<T>
#elif defined(NETIO_HAS_PTHREADS)
  : public posix_tss_ptr<T>
#endif
{
public:
  void operator=(T* value)
  {
#if !defined(NETIO_HAS_THREADS)
    null_tss_ptr<T>::operator=(value);
#elif defined(NETIO_HAS_THREAD_KEYWORD_EXTENSION)
    keyword_tss_ptr<T>::operator=(value);
#elif defined(NETIO_WINDOWS)
    win_tss_ptr<T>::operator=(value);
#elif defined(NETIO_HAS_PTHREADS)
    posix_tss_ptr<T>::operator=(value);
#endif
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_TSS_PTR_HPP
