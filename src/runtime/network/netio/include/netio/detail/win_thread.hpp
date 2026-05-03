#ifndef NETIO_DETAIL_WIN_THREAD_HPP
#define NETIO_DETAIL_WIN_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS) \
  && !defined(NETIO_WINDOWS_APP) \
  && !defined(UNDER_CE)

#include <cstddef>
#include "netio/detail/memory.hpp"
#include "netio/detail/socket_types.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

NETIO_DECL unsigned int __stdcall win_thread_function(void* arg);

#if defined(WINVER) && (WINVER < 0x0500)
NETIO_DECL void __stdcall apc_function(ULONG data);
#else
NETIO_DECL void __stdcall apc_function(ULONG_PTR data);
#endif

template <typename T>
class win_thread_base
{
public:
  static bool terminate_threads()
  {
    return ::InterlockedExchangeAdd(&terminate_threads_, 0) != 0;
  }

  static void set_terminate_threads(bool b)
  {
    ::InterlockedExchange(&terminate_threads_, b ? 1 : 0);
  }

private:
  static long terminate_threads_;
};

template <typename T>
long win_thread_base<T>::terminate_threads_ = 0;

class win_thread
  : public win_thread_base<win_thread>
{
public:
  // Construct in a non-joinable state.
  win_thread() noexcept
    : arg_(0)
  {
  }

  // Constructor.
  template <typename Function>
  win_thread(Function f, unsigned int stack_size = 0)
    : win_thread(std::allocator_arg, std::allocator<void>(), f, stack_size)
  {
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  win_thread(allocator_arg_t, const Allocator& a,
      Function f, unsigned int stack_size = 0)
    : arg_(start_thread(allocate_object<func<Function, Allocator>>(a, f, a),
          stack_size))
  {
  }

  // Move constructor.
  win_thread(win_thread&& other) noexcept
    : arg_(other.arg_)
  {
    other.arg_ = 0;
  }

  // Destructor.
  NETIO_DECL ~win_thread();

  // Move assignment.
  win_thread& operator=(win_thread&& other) noexcept
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
  friend NETIO_DECL unsigned int __stdcall win_thread_function(void* arg);

#if defined(WINVER) && (WINVER < 0x0500)
  friend NETIO_DECL void __stdcall apc_function(ULONG);
#else
  friend NETIO_DECL void __stdcall apc_function(ULONG_PTR);
#endif

  class func_base
  {
  public:
    virtual ~func_base() {}
    virtual void run() = 0;
    virtual void destroy() = 0;
    ::HANDLE thread_;
    ::HANDLE entry_event_;
    ::HANDLE exit_event_;
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

  NETIO_DECL func_base* start_thread(
      func_base* arg, unsigned int stack_size);

  func_base* arg_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#if defined(NETIO_HEADER_ONLY)
# include "netio/detail/impl/win_thread.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_WINDOWS)
       // && !defined(NETIO_WINDOWS_APP)
       // && !defined(UNDER_CE)

#endif // NETIO_DETAIL_WIN_THREAD_HPP
