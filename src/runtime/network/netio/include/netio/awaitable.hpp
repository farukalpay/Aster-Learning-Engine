#ifndef NETIO_AWAITABLE_HPP
#define NETIO_AWAITABLE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_HAS_CO_AWAIT) || defined(GENERATING_DOCUMENTATION)

#if defined(NETIO_HAS_STD_COROUTINE)
# include <coroutine>
#else // defined(NETIO_HAS_STD_COROUTINE)
# include <experimental/coroutine>
#endif // defined(NETIO_HAS_STD_COROUTINE)

#include <utility>
#include "netio/any_io_executor.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

#if defined(NETIO_HAS_STD_COROUTINE)
using std::coroutine_handle;
using std::suspend_always;
#else // defined(NETIO_HAS_STD_COROUTINE)
using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
#endif // defined(NETIO_HAS_STD_COROUTINE)

template <typename> class awaitable_thread;
template <typename, typename> class awaitable_frame;

} // namespace detail

/// The return type of a coroutine or asynchronous operation.
template <typename T, typename Executor = any_io_executor>
class NETIO_NODISCARD awaitable
{
public:
  /// The type of the awaited value.
  typedef T value_type;

  /// The executor type that will be used for the coroutine.
  typedef Executor executor_type;

  /// Default constructor.
  constexpr awaitable() noexcept
    : frame_(nullptr)
  {
  }

  /// Move constructor.
  awaitable(awaitable&& other) noexcept
    : frame_(std::exchange(other.frame_, nullptr))
  {
  }

  /// Destructor
  ~awaitable()
  {
    if (frame_)
      frame_->destroy();
  }

  /// Move assignment.
  awaitable& operator=(awaitable&& other) noexcept
  {
    if (this != &other)
    {
      if (frame_)
        frame_->destroy();
      frame_ = std::exchange(other.frame_, nullptr);
    }
    return *this;
  }

  /// Checks if the awaitable refers to a future result.
  bool valid() const noexcept
  {
    return !!frame_;
  }

#if !defined(GENERATING_DOCUMENTATION)

  // Support for co_await keyword.
  bool await_ready() const noexcept
  {
    return false;
  }

  // Support for co_await keyword.
  template <class U>
  void await_suspend(
      detail::coroutine_handle<detail::awaitable_frame<U, Executor>> h)
  {
    frame_->push_frame(&h.promise());
  }

  // Support for co_await keyword.
  T await_resume()
  {
    return awaitable(static_cast<awaitable&&>(*this)).frame_->get();
  }

#endif // !defined(GENERATING_DOCUMENTATION)

private:
  template <typename> friend class detail::awaitable_thread;
  template <typename, typename> friend class detail::awaitable_frame;

  // Not copy constructible or copy assignable.
  awaitable(const awaitable&) = delete;
  awaitable& operator=(const awaitable&) = delete;

  // Construct the awaitable from a coroutine's frame object.
  explicit awaitable(detail::awaitable_frame<T, Executor>* a)
    : frame_(a)
  {
  }

  detail::awaitable_frame<T, Executor>* frame_;
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/awaitable.hpp"
#if defined(NETIO_HEADER_ONLY)
# include "netio/impl/awaitable.ipp"
#endif // defined(NETIO_HEADER_ONLY)

#endif // defined(NETIO_HAS_CO_AWAIT) || defined(GENERATING_DOCUMENTATION)

#endif // NETIO_AWAITABLE_HPP
