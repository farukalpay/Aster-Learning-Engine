#ifndef NETIO_DETAIL_POSIX_THREAD_HPP
#define NETIO_DETAIL_POSIX_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_PTHREADS)

#include <cstddef>
#include <pthread.h>
#include "netio/detail/memory.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

extern "C"
{
  NETIO_DECL void* netio_detail_posix_thread_function(void* arg);
}

class posix_thread
{
public:
  // Construct in a non-joinable state.
  posix_thread() noexcept
    : arg_(0)
  {
  }

  // Constructor.
  template <typename Function>
  posix_thread(Function f, unsigned int = 0)
    : posix_thread(std::allocator_arg, std::allocator<void>(), f)
  {
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  posix_thread(allocator_arg_t, const Allocator& a,
      Function f, unsigned int = 0)
    : arg_(start_thread(allocate_object<func<Function, Allocator>>(a, f, a)))
  {
  }

  // Move constructor.
  posix_thread(posix_thread&& other) noexcept
    : arg_(other.arg_)
  {
    other.arg_ = 0;
  }

  // Destructor.
  NETIO_DECL ~posix_thread();

  // Move assignment.
  posix_thread& operator=(posix_thread&& other) noexcept
  {
    arg_ = other.arg_;
    other.arg_ = 0;
    return *this;
  }

  // Whether the thread can be joined.
  bool joinable() const
  {
    return !!arg_;
  }

  // Wait for the thread to exit.
  NETIO_DECL void join();

  // Get number of CPUs.
  NETIO_DECL static std::size_t hardware_concurrency();

private:
  friend void* netio_detail_posix_thread_function(void* arg);

  class func_base
  {
  public:
    virtual ~func_base() {}
    virtual void run() = 0;
    virtual void destroy() = 0;
    ::pthread_t thread_;
  };

  template <typename Function, typename Allocator>
  class func
    : public func_base
  {
  public:
    func(Function f, const Allocator& a)
      : f_(f),
        allocator_(a)
    {
    }

    virtual void run()
    {
      f_();
    }

    virtual void destroy()
    {
      deallocate_object(allocator_, this);
    }

  private:
    Function f_;
    Allocator allocator_;
  };

  NETIO_DECL func_base* start_thread(func_base* arg);

  func_base* arg_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/posix_thread.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_PTHREADS)

#endif // NETIO_DETAIL_POSIX_THREAD_HPP
