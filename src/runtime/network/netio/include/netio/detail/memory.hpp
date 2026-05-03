#ifndef NETIO_DETAIL_MEMORY_HPP
#define NETIO_DETAIL_MEMORY_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include "netio/detail/cstdint.hpp"
#include "netio/detail/throw_exception.hpp"

#if !defined(NETIO_HAS_STD_ALIGNED_ALLOC) \
  && defined(NETIO_HAS_BOOST_ALIGN)
# include <boost/align/aligned_alloc.hpp>
#endif // !defined(NETIO_HAS_STD_ALIGNED_ALLOC)
       //   && defined(NETIO_HAS_BOOST_ALIGN)

namespace netio {
namespace detail {

using std::allocate_shared;
using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;
using std::addressof;

#if defined(NETIO_HAS_STD_TO_ADDRESS)
using std::to_address;
#else // defined(NETIO_HAS_STD_TO_ADDRESS)
template <typename T>
inline T* to_address(T* p) { return p; }
template <typename T>
inline const T* to_address(const T* p) { return p; }
template <typename T>
inline volatile T* to_address(volatile T* p) { return p; }
template <typename T>
inline const volatile T* to_address(const volatile T* p) { return p; }
#endif // defined(NETIO_HAS_STD_TO_ADDRESS)

inline void* align(std::size_t alignment,
    std::size_t size, void*& ptr, std::size_t& space)
{
  return std::align(alignment, size, ptr, space);
}

template <typename T, typename Allocator, typename... Args>
T* allocate_object(const Allocator& a, Args&&... args)
{
  typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(a);
  T* raw = std::allocator_traits<decltype(alloc)>::allocate(alloc, 1);
#if !defined(NETIO_NO_EXCEPTIONS)
  try
#endif // !defined(NETIO_NO_EXCEPTIONS)
  {
    return new (raw) T(static_cast<Args&&>(args)...);
  }
#if !defined(NETIO_NO_EXCEPTIONS)
  catch (...)
  {
    std::allocator_traits<decltype(alloc)>::deallocate(alloc, raw, 1);
    throw;
  }
#endif // !defined(NETIO_NO_EXCEPTIONS)
}

template <typename Allocator, typename T>
void deallocate_object(const Allocator& a, T* ptr)
{
  typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(a);
  std::allocator_traits<decltype(alloc)>::destroy(alloc, ptr);
  std::allocator_traits<decltype(alloc)>::deallocate(alloc, ptr, 1);
}

} // namespace detail

using std::allocator_arg_t;
# define NETIO_USES_ALLOCATOR(t) \
  namespace std { \
    template <typename Allocator> \
    struct uses_allocator<t, Allocator> : true_type {}; \
  } \
  /**/
# define NETIO_REBIND_ALLOC(alloc, t) \
  typename std::allocator_traits<alloc>::template rebind_alloc<t>
  /**/

inline void* aligned_new(std::size_t align, std::size_t size)
{
#if defined(NETIO_HAS_STD_ALIGNED_ALLOC)
  align = (align < NETIO_DEFAULT_ALIGN) ? NETIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = std::aligned_alloc(align, size);
  if (!ptr)
  {
    std::bad_alloc ex;
    netio::detail::throw_exception(ex);
  }
  return ptr;
#elif defined(NETIO_HAS_BOOST_ALIGN)
  align = (align < NETIO_DEFAULT_ALIGN) ? NETIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = boost::alignment::aligned_alloc(align, size);
  if (!ptr)
  {
    std::bad_alloc ex;
    netio::detail::throw_exception(ex);
  }
  return ptr;
#elif defined(NETIO_MSVC)
  align = (align < NETIO_DEFAULT_ALIGN) ? NETIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = _aligned_malloc(size, align);
  if (!ptr)
  {
    std::bad_alloc ex;
    netio::detail::throw_exception(ex);
  }
  return ptr;
#else // defined(NETIO_MSVC)
  (void)align;
  return ::operator new(size);
#endif // defined(NETIO_MSVC)
}

inline void aligned_delete(void* ptr)
{
#if defined(NETIO_HAS_STD_ALIGNED_ALLOC)
  std::free(ptr);
#elif defined(NETIO_HAS_BOOST_ALIGN)
  boost::alignment::aligned_free(ptr);
#elif defined(NETIO_MSVC)
  _aligned_free(ptr);
#else // defined(NETIO_MSVC)
  ::operator delete(ptr);
#endif // defined(NETIO_MSVC)
}

} // namespace netio

#endif // NETIO_DETAIL_MEMORY_HPP
