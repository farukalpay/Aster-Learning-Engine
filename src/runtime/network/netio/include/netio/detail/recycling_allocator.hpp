#ifndef NETIO_DETAIL_RECYCLING_ALLOCATOR_HPP
#define NETIO_DETAIL_RECYCLING_ALLOCATOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/thread_context.hpp"
#include "netio/detail/thread_info_base.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename T, typename Purpose = thread_info_base::default_tag>
class recycling_allocator
{
public:
  typedef T value_type;

  template <typename U>
  struct rebind
  {
    typedef recycling_allocator<U, Purpose> other;
  };

  recycling_allocator()
  {
  }

  template <typename U>
  recycling_allocator(const recycling_allocator<U, Purpose>&)
  {
  }

  T* allocate(std::size_t n)
  {
#if !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
    void* p = thread_info_base::allocate(Purpose(),
        thread_context::top_of_thread_call_stack(),
        sizeof(T) * n, alignof(T));
#else // !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
    void* p = netio::aligned_new(alignof(T), sizeof(T) * n);
#endif // !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
    return static_cast<T*>(p);
  }

  void deallocate(T* p, std::size_t n)
  {
#if !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
    thread_info_base::deallocate(Purpose(),
        thread_context::top_of_thread_call_stack(), p, sizeof(T) * n);
#else // !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
    (void)n;
    netio::aligned_delete(p);
#endif // !defined(NETIO_DISABLE_SMALL_BLOCK_RECYCLING)
  }
};

template <typename Purpose>
class recycling_allocator<void, Purpose>
{
public:
  typedef void value_type;

  template <typename U>
  struct rebind
  {
    typedef recycling_allocator<U, Purpose> other;
  };

  recycling_allocator()
  {
  }

  template <typename U>
  recycling_allocator(const recycling_allocator<U, Purpose>&)
  {
  }
};

template <typename Allocator, typename Purpose>
struct get_recycling_allocator
{
  typedef Allocator type;
  static type get(const Allocator& a) { return a; }
};

template <typename T, typename Purpose>
struct get_recycling_allocator<std::allocator<T>, Purpose>
{
  typedef recycling_allocator<T, Purpose> type;
  static type get(const std::allocator<T>&) { return type(); }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_RECYCLING_ALLOCATOR_HPP
