#ifndef NETIO_SYSTEM_EXECUTOR_HPP
#define NETIO_SYSTEM_EXECUTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/execution.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

class system_context;

/// An executor that uses arbitrary threads.
/**
 * The system executor represents an execution context where functions are
 * permitted to run on arbitrary threads. When the blocking.never property is
 * established, the system executor will schedule the function to run on an
 * unspecified system thread pool. When either blocking.possibly or
 * blocking.always is established, the executor invokes the function
 * immediately.
 */
template <typename Blocking, typename Relationship, typename Allocator>
class basic_system_executor
{
public:
  /// Default constructor.
  basic_system_executor() noexcept
    : allocator_(Allocator())
  {
  }

#if !defined(GENERATING_DOCUMENTATION)
private:
  friend struct netio_require_fn::impl;
  friend struct netio_prefer_fn::impl;
#endif // !defined(GENERATING_DOCUMENTATION)

  /// Obtain an executor with the @c blocking.possibly property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::blocking.possibly); @endcode
   */
  basic_system_executor<execution::blocking_t::possibly_t,
      Relationship, Allocator>
  require(execution::blocking_t::possibly_t) const
  {
    return basic_system_executor<execution::blocking_t::possibly_t,
        Relationship, Allocator>(allocator_);
  }

  /// Obtain an executor with the @c blocking.always property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::blocking.always); @endcode
   */
  basic_system_executor<execution::blocking_t::always_t,
      Relationship, Allocator>
  require(execution::blocking_t::always_t) const
  {
    return basic_system_executor<execution::blocking_t::always_t,
        Relationship, Allocator>(allocator_);
  }

  /// Obtain an executor with the @c blocking.never property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::blocking.never); @endcode
   */
  basic_system_executor<execution::blocking_t::never_t,
      Relationship, Allocator>
  require(execution::blocking_t::never_t) const
  {
    return basic_system_executor<execution::blocking_t::never_t,
        Relationship, Allocator>(allocator_);
  }

  /// Obtain an executor with the @c relationship.continuation property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::relationship.continuation); @endcode
   */
  basic_system_executor<Blocking,
      execution::relationship_t::continuation_t, Allocator>
  require(execution::relationship_t::continuation_t) const
  {
    return basic_system_executor<Blocking,
        execution::relationship_t::continuation_t, Allocator>(allocator_);
  }

  /// Obtain an executor with the @c relationship.fork property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::relationship.fork); @endcode
   */
  basic_system_executor<Blocking,
      execution::relationship_t::fork_t, Allocator>
  require(execution::relationship_t::fork_t) const
  {
    return basic_system_executor<Blocking,
        execution::relationship_t::fork_t, Allocator>(allocator_);
  }

  /// Obtain an executor with the specified @c allocator property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::allocator(my_allocator)); @endcode
   */
  template <typename OtherAllocator>
  basic_system_executor<Blocking, Relationship, OtherAllocator>
  require(execution::allocator_t<OtherAllocator> a) const
  {
    return basic_system_executor<Blocking,
        Relationship, OtherAllocator>(a.value());
  }

  /// Obtain an executor with the default @c allocator property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::require customisation point.
   *
   * For example:
   * @code netio::system_executor ex1;
   * auto ex2 = netio::require(ex1,
   *     netio::execution::allocator); @endcode
   */
  basic_system_executor<Blocking, Relationship, std::allocator<void>>
  require(execution::allocator_t<void>) const
  {
    return basic_system_executor<Blocking,
        Relationship, std::allocator<void>>();
  }

#if !defined(GENERATING_DOCUMENTATION)
private:
  friend struct netio_query_fn::impl;
  friend struct netio::execution::detail::blocking_t<0>;
  friend struct netio::execution::detail::mapping_t<0>;
  friend struct netio::execution::detail::inline_exception_handling_t<0>;
  friend struct netio::execution::detail::outstanding_work_t<0>;
  friend struct netio::execution::detail::relationship_t<0>;
#endif // !defined(GENERATING_DOCUMENTATION)

  /// Query the current value of the @c mapping property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
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
   * @code netio::system_executor ex;
   * if (netio::query(ex,
   *       netio::execution::inline_exception_handling)
   *     == netio::execution::inline_exception_handling.thread)
   *   ... @endcode
   */
  static constexpr execution::inline_exception_handling_t query(
      execution::inline_exception_handling_t) noexcept
  {
    return execution::inline_exception_handling.terminate;
  }

  /// Query the current value of the @c context property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * netio::system_context& pool = netio::query(
   *     ex, netio::execution::context); @endcode
   */
  static system_context& query(execution::context_t) noexcept;

  /// Query the current value of the @c blocking property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * if (netio::query(ex, netio::execution::blocking)
   *       == netio::execution::blocking.always)
   *   ... @endcode
   */
  static constexpr execution::blocking_t query(
      execution::blocking_t) noexcept
  {
    return Blocking();
  }

  /// Query the current value of the @c relationship property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * if (netio::query(ex, netio::execution::relationship)
   *       == netio::execution::relationship.continuation)
   *   ... @endcode
   */
  static constexpr execution::relationship_t query(
      execution::relationship_t) noexcept
  {
    return Relationship();
  }

  /// Query the current value of the @c allocator property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * auto alloc = netio::query(ex,
   *     netio::execution::allocator); @endcode
   */
  template <typename OtherAllocator>
  constexpr Allocator query(
      execution::allocator_t<OtherAllocator>) const noexcept
  {
    return allocator_;
  }

  /// Query the current value of the @c allocator property.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * auto alloc = netio::query(ex,
   *     netio::execution::allocator); @endcode
   */
  constexpr Allocator query(
      execution::allocator_t<void>) const noexcept
  {
    return allocator_;
  }

  /// Query the occupancy (recommended number of work items) for the system
  /// context.
  /**
   * Do not call this function directly. It is intended for use with the
   * netio::query customisation point.
   *
   * For example:
   * @code netio::system_executor ex;
   * std::size_t occupancy = netio::query(
   *     ex, netio::execution::occupancy); @endcode
   */
  std::size_t query(execution::occupancy_t) const noexcept;

public:
  /// Compare two executors for equality.
  /**
   * Two executors are equal if they refer to the same underlying io_context.
   */
  friend bool operator==(const basic_system_executor&,
      const basic_system_executor&) noexcept
  {
    return true;
  }

  /// Compare two executors for inequality.
  /**
   * Two executors are equal if they refer to the same underlying io_context.
   */
  friend bool operator!=(const basic_system_executor&,
      const basic_system_executor&) noexcept
  {
    return false;
  }

  /// Execution function.
  template <typename Function>
  void execute(Function&& f) const
  {
    this->do_execute(static_cast<Function&&>(f), Blocking());
  }

#if !defined(NETIO_NO_TS_EXECUTORS)
public:
  /// Obtain the underlying execution context.
  system_context& context() const noexcept;

  /// Inform the executor that it has some outstanding work to do.
  /**
   * For the system executor, this is a no-op.
   */
  void on_work_started() const noexcept
  {
  }

  /// Inform the executor that some work is no longer outstanding.
  /**
   * For the system executor, this is a no-op.
   */
  void on_work_finished() const noexcept
  {
  }

  /// Request the system executor to invoke the given function object.
  /**
   * This function is used to ask the executor to execute the given function
   * object. The function object will always be executed inside this function.
   *
   * @param f The function object to be called. The executor will make
   * a copy of the handler object as required. The function signature of the
   * function object must be: @code void function(); @endcode
   *
   * @param a An allocator that may be used by the executor to allocate the
   * internal storage needed for function invocation.
   */
  template <typename Function, typename OtherAllocator>
  void dispatch(Function&& f, const OtherAllocator& a) const;

  /// Request the system executor to invoke the given function object.
  /**
   * This function is used to ask the executor to execute the given function
   * object. The function object will never be executed inside this function.
   * Instead, it will be scheduled to run on an unspecified system thread pool.
   *
   * @param f The function object to be called. The executor will make
   * a copy of the handler object as required. The function signature of the
   * function object must be: @code void function(); @endcode
   *
   * @param a An allocator that may be used by the executor to allocate the
   * internal storage needed for function invocation.
   */
  template <typename Function, typename OtherAllocator>
  void post(Function&& f, const OtherAllocator& a) const;

  /// Request the system executor to invoke the given function object.
  /**
   * This function is used to ask the executor to execute the given function
   * object. The function object will never be executed inside this function.
   * Instead, it will be scheduled to run on an unspecified system thread pool.
   *
   * @param f The function object to be called. The executor will make
   * a copy of the handler object as required. The function signature of the
   * function object must be: @code void function(); @endcode
   *
   * @param a An allocator that may be used by the executor to allocate the
   * internal storage needed for function invocation.
   */
  template <typename Function, typename OtherAllocator>
  void defer(Function&& f, const OtherAllocator& a) const;
#endif // !defined(NETIO_NO_TS_EXECUTORS)

private:
  template <typename, typename, typename> friend class basic_system_executor;

  // Constructor used by require().
  basic_system_executor(const Allocator& a)
    : allocator_(a)
  {
  }

  /// Execution helper implementation for the possibly blocking property.
  template <typename Function>
  void do_execute(Function&& f,
      execution::blocking_t::possibly_t) const;

  /// Execution helper implementation for the always blocking property.
  template <typename Function>
  void do_execute(Function&& f,
      execution::blocking_t::always_t) const;

  /// Execution helper implementation for the never blocking property.
  template <typename Function>
  void do_execute(Function&& f,
      execution::blocking_t::never_t) const;

  // The allocator used for execution functions.
  Allocator allocator_;
};

/// An executor that uses arbitrary threads.
/**
 * The system executor represents an execution context where functions are
 * permitted to run on arbitrary threads. When the blocking.never property is
 * established, the system executor will schedule the function to run on an
 * unspecified system thread pool. When either blocking.possibly or
 * blocking.always is established, the executor invokes the function
 * immediately.
 */
typedef basic_system_executor<execution::blocking_t::possibly_t,
    execution::relationship_t::fork_t, std::allocator<void>>
  system_executor;

#if !defined(GENERATING_DOCUMENTATION)

namespace traits {

#if !defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

template <typename Blocking, typename Relationship, typename Allocator>
struct equality_comparable<
    netio::basic_system_executor<Blocking, Relationship, Allocator>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
};

#endif // !defined(NETIO_HAS_DEDUCED_EQUALITY_COMPARABLE_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

template <typename Blocking, typename Relationship,
    typename Allocator, typename Function>
struct execute_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    Function
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef void result_type;
};

#endif // !defined(NETIO_HAS_DEDUCED_EXECUTE_MEMBER_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::blocking_t::possibly_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<
      netio::execution::blocking_t::possibly_t,
      Relationship, Allocator> result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::blocking_t::always_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<
      netio::execution::blocking_t::always_t,
      Relationship, Allocator> result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::blocking_t::never_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<
      netio::execution::blocking_t::never_t,
      Relationship, Allocator> result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::relationship_t::fork_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<Blocking,
      netio::execution::relationship_t::fork_t,
      Allocator> result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::relationship_t::continuation_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<Blocking,
      netio::execution::relationship_t::continuation_t,
      Allocator> result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::allocator_t<void>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<Blocking,
      Relationship, std::allocator<void>> result_type;
};

template <typename Blocking, typename Relationship,
    typename Allocator, typename OtherAllocator>
struct require_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::allocator_t<OtherAllocator>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = false;
  typedef netio::basic_system_executor<Blocking,
      Relationship, OtherAllocator> result_type;
};

#endif // !defined(NETIO_HAS_DEDUCED_REQUIRE_MEMBER_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

template <typename Blocking, typename Relationship,
    typename Allocator, typename Property>
struct query_static_constexpr_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    Property,
    typename netio::enable_if<
      netio::is_convertible<
        Property,
        netio::execution::mapping_t
      >::value
    >::type
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef netio::execution::mapping_t::thread_t result_type;

  static constexpr result_type value() noexcept
  {
    return result_type();
  }
};

template <typename Blocking, typename Relationship,
    typename Allocator, typename Property>
struct query_static_constexpr_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    Property,
    typename netio::enable_if<
      netio::is_convertible<
        Property,
        netio::execution::inline_exception_handling_t
      >::value
    >::type
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef netio::execution::inline_exception_handling_t::terminate_t
    result_type;

  static constexpr result_type value() noexcept
  {
    return result_type();
  }
};

#endif // !defined(NETIO_HAS_DEDUCED_QUERY_STATIC_CONSTEXPR_MEMBER_TRAIT)

#if !defined(NETIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

template <typename Blocking, typename Relationship,
    typename Allocator, typename Property>
struct query_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    Property,
    typename netio::enable_if<
      netio::is_convertible<
        Property,
        netio::execution::blocking_t
      >::value
    >::type
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef netio::execution::blocking_t result_type;
};

template <typename Blocking, typename Relationship,
    typename Allocator, typename Property>
struct query_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    Property,
    typename netio::enable_if<
      netio::is_convertible<
        Property,
        netio::execution::relationship_t
      >::value
    >::type
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef netio::execution::relationship_t result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct query_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::context_t
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef netio::system_context& result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct query_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::allocator_t<void>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef Allocator result_type;
};

template <typename Blocking, typename Relationship, typename Allocator>
struct query_member<
    netio::basic_system_executor<Blocking, Relationship, Allocator>,
    netio::execution::allocator_t<Allocator>
  >
{
  static constexpr bool is_valid = true;
  static constexpr bool is_noexcept = true;
  typedef Allocator result_type;
};

#endif // !defined(NETIO_HAS_DEDUCED_QUERY_MEMBER_TRAIT)

} // namespace traits

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/system_executor.hpp"

#endif // NETIO_SYSTEM_EXECUTOR_HPP
