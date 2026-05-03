#ifndef NETIO_INLINE_EXECUTOR_HPP
#define NETIO_INLINE_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/non_const_lvalue.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/execution.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// An executor that always executes the function object inline.
template <typename InlineExceptionHandling>
class basic_inline_executor
{
public:
  /// Default constructor.
  basic_inline_executor() noexcept
  {
  }

#if !defined(GENERATING_DOCUMENTATION)
private:
  friend struct netio_require_fn::impl;
  friend struct netio_prefer_fn::impl;
#endif // !defined(GENERATING_DOCUMENTATION)

  /// Obtain an executor with the @c inline_exception_handling.propagate
  /// property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::inline_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::inline_exception_handling.propagate); @endcode
   */
  basic_inline_executor<execution::inline_exception_handling_t::propagate_t>
  require(execution::inline_exception_handling_t::propagate_t) const noexcept
  {
    return basic_inline_executor<
        execution::inline_exception_handling_t::propagate_t>();
  }

  /// Obtain an executor with the @c inline_exception_handling.terminate
  /// property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::inline_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::inline_exception_handling.terminate); @endcode
   */
  basic_inline_executor<execution::inline_exception_handling_t::terminate_t>
  require(execution::inline_exception_handling_t::terminate_t) const noexcept
  {
    return basic_inline_executor<
        execution::inline_exception_handling_t::terminate_t>();
  }

#if !defined(GENERATING_DOCUMENTATION)
private:
  friend struct netio_query_fn::impl;
  friend struct netio::execution::detail::blocking_t<0>;
  friend struct netio::execution::detail::mapping_t<0>;
  friend struct netio::execution::detail::inline_exception_handling_t<0>;
#endif // !defined(GENERATING_DOCUMENTATION)

  /// Query the current value of the @c mapping property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::inline_executor ex;
   * if (netio::query(ex, netio::execution::mapping)
   *       == netio::execution::mapping.thread)
   *   ... @endcode
   */
  static constexpr execution::mapping_t query(
      execution::mapping_t) noexcept
  {
    return execution::mapping.thread;
  }

  /// Query the current value of the @c inline_exception_handling property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::inline_executor ex;
   * if (netio::query(ex,
   *       netio::execution::inline_exception_handling)
   *     == netio::execution::inline_exception_handling.propagate)
   *   ... @endcode
   */
  static constexpr execution::inline_exception_handling_t query(
      execution::inline_exception_handling_t) noexcept
  {
    return InlineExceptionHandling();
  }

  /// Query the current value of the @c blocking property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::inline_executor ex;
   * if (netio::query(ex, netio::execution::blocking)
   *     == netio::execution::blocking.always)
   *   ... @endcode
   */
  static constexpr execution::blocking_t query(
      execution::blocking_t) noexcept
  {
    return execution::blocking.always;
  }

public:
  /// Compare two executors for equality.
  /**
   * Two inline executors are always considered equal.
   */
  friend bool operator==(const basic_inline_executor&,
      const basic_inline_executor&) noexcept
  {
    return true;
  }

  /// Compare two executors for inequality.
  /**
   * Two inline executors are never considered unequal.
   */
  friend bool operator!=(const basic_inline_executor&,
      const basic_inline_executor&) noexcept
  {
    return false;
  }

  /// Execution function.
  template <typename Function>
  void execute(Function&& f) const
  {
#if !defined(NETIO_NO_EXCEPTIONS)
    try
#endif // !defined(NETIO_NO_EXCEPTIONS)
    {
      detail::non_const_lvalue<Function> f2(f);
      static_cast<decay_t<Function>&&>(f2.value)();
    }
#if !defined(NETIO_NO_EXCEPTIONS)
    catch (...)
    {
      if (is_same<InlineExceptionHandling,
          execution::inline_exception_handling_t::terminate_t>::value)
      {
        std::terminate();
      }
      else
      {
        throw;
      }
    }
#endif // !defined(NETIO_NO_EXCEPTIONS)
  }
};

/// An executor that always executes the function object inline.
typedef basic_inline_executor<
  execution::inline_exception_handling_t::propagate_t>
    inline_executor;

#if !defined(GENERATING_DOCUMENTATION)

namespace traits {

#if !defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <typename InlineExceptionHandling>
struct equality_comparable<
    basic_inline_executor<InlineExceptionHandling>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
};

#endif // !defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename InlineExceptionHandling, typename Function>
struct execute_member<
    basic_inline_executor<InlineExceptionHandling>,
    Function
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef void result_type;
};

#endif // !defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

template <typename InlineExceptionHandling>
struct require_member<
    basic_inline_executor<InlineExceptionHandling>,
    execution::inline_exception_handling_t::propagate_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef basic_inline_executor<
      execution::inline_exception_handling_t::propagate_t>
        result_type;
};

template <typename InlineExceptionHandling>
struct require_member<
    basic_inline_executor<InlineExceptionHandling>,
    execution::inline_exception_handling_t::terminate_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef basic_inline_executor<
      execution::inline_exception_handling_t::terminate_t>
        result_type;
};

#endif // !defined(NETIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

template <typename InlineExceptionHandling, typename Property>
struct query_static_constexpr_member<
    basic_inline_executor<InlineExceptionHandling>,
    Property,
    enable_if_t<
      is_convertible<
        Property,
        execution::mapping_t
      >::value
    >
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef execution::mapping_t::thread_t result_type;

  static constexpr result_type value() noexcept
  {
    return result_type();
  }
};

template <typename InlineExceptionHandling, typename Property>
struct query_static_constexpr_member<
    basic_inline_executor<InlineExceptionHandling>,
    Property,
    enable_if_t<
      is_convertible<
        Property,
        execution::inline_exception_handling_t
      >::value
    >
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef InlineExceptionHandling result_type;

  static constexpr result_type value() noexcept
  {
    return result_type();
  }
};

template <typename InlineExceptionHandling, typename Property>
struct query_static_constexpr_member<
    basic_inline_executor<InlineExceptionHandling>,
    Property,
    enable_if_t<
      is_convertible<
        Property,
        execution::blocking_t
      >::value
    >
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef execution::blocking_t::always_t result_type;

  static constexpr result_type value() noexcept
  {
    return result_type();
  }
};

#endif // !defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

} // namespace traits

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_INLINE_EXECUTOR_HPP
