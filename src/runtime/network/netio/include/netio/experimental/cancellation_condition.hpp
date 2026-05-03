#ifndef NETIO_EXPERIMENTAL_CANCELLATION_CONDITION_HPP
#define NETIO_EXPERIMENTAL_CANCELLATION_CONDITION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/cancellation_type.hpp"
#include "netio/disposition.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace experimental {

/// Wait for all operations to complete.
class wait_for_all
{
public:
  template <typename... Args>
  constexpr cancellation_type_t operator()(Args&&...) const noexcept
  {
    return cancellation_type::none;
  }
};

/// Wait until an operation completes, then cancel the others.
class wait_for_one
{
public:
  constexpr explicit wait_for_one(
      cancellation_type_t cancel_type = cancellation_type::all)
    : cancel_type_(cancel_type)
  {
  }

  template <typename... Args>
  constexpr cancellation_type_t operator()(Args&&...) const noexcept
  {
    return cancel_type_;
  }

private:
  cancellation_type_t cancel_type_;
};

/// Wait until an operation completes without an error, then cancel the others.
/**
 * If no operation completes without an error, waits for completion of all
 * operations.
 */
class wait_for_one_success
{
public:
  constexpr explicit wait_for_one_success(
      cancellation_type_t cancel_type = cancellation_type::all)
    : cancel_type_(cancel_type)
  {
  }

  constexpr cancellation_type_t
  operator()() const noexcept
  {
    return cancel_type_;
  }

  template <typename E, typename... Args>
  constexpr constraint_t<!is_disposition<E>::value, cancellation_type_t>
  operator()(const E&, Args&&...) const noexcept
  {
    return cancel_type_;
  }

  template <typename E, typename... Args>
  constexpr constraint_t<is_disposition<E>::value, cancellation_type_t>
  operator()(const E& e, Args&&...) const noexcept
  {
    return e != no_error ? cancellation_type::none : cancel_type_;
  }

private:
  cancellation_type_t cancel_type_;
};

/// Wait until an operation completes with an error, then cancel the others.
/**
 * If no operation completes with an error, waits for completion of all
 * operations.
 */
class wait_for_one_error
{
public:
  constexpr explicit wait_for_one_error(
      cancellation_type_t cancel_type = cancellation_type::all)
    : cancel_type_(cancel_type)
  {
  }

  constexpr cancellation_type_t operator()() const noexcept
  {
    return cancellation_type::none;
  }

  template <typename E, typename... Args>
  constexpr constraint_t<!is_disposition<E>::value, cancellation_type_t>
  operator()(const E&, Args&&...) const noexcept
  {
    return cancellation_type::none;
  }

  template <typename E, typename... Args>
  constexpr constraint_t<is_disposition<E>::value, cancellation_type_t>
  operator()(const E& e, Args&&...) const noexcept
  {
    return e != no_error ? cancel_type_ : cancellation_type::none;
  }

private:
  cancellation_type_t cancel_type_;
};

} // namespace experimental
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_EXPERIMENTAL_CANCELLATION_CONDITION_HPP
