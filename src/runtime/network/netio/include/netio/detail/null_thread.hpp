#ifndef NETIO_DETAIL_NULL_THREAD_HPP
#define NETIO_DETAIL_NULL_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)

#include "netio/detail/throw_error.hpp"
#include "netio/error.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

class null_thread
{
public:
  // Construct in a non-joinable state.
  null_thread() noexcept
  {
  }

  // Constructor.
  template <typename Function>
  null_thread(Function, unsigned int = 0)
  {
    netio::detail::throw_error(
        netio::error::operation_not_supported, "thread");
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  null_thread(allocator_arg_t, const Allocator&, Function, unsigned int = 0)
  {
    netio::detail::throw_error(
        netio::error::operation_not_supported, "thread");
  }

  // Move constructor.
  null_thread(null_thread&&) noexcept
  {
  }

  // Destructor.
  ~null_thread()
  {
  }

  // Move assignment.
  null_thread& operator=(null_thread&&) noexcept
  {
    return *this;
  }

  // Whether the thread can be joined.
  bool joinable() const
  {
    return false;
  }

  // Wait for the thread to exit.
  void join()
  {
  }

  // Get number of CPUs.
  static std::size_t hardware_concurrency()
  {
    return 1;
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // !defined(NETIO_HAS_THREADS)

#endif // NETIO_DETAIL_NULL_THREAD_HPP
