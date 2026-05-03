#ifndef NETIO_DETAIL_ATOMIC_COUNT_HPP
#define NETIO_DETAIL_ATOMIC_COUNT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if !defined(NETIO_HAS_THREADS)
// Nothing to include.
#else // !defined(NETIO_HAS_THREADS)
# include <atomic>
#endif // !defined(NETIO_HAS_THREADS)

namespace netio {
namespace detail {

#if !defined(NETIO_HAS_THREADS)
typedef long atomic_count;
inline void increment(atomic_count& a, long b) { a += b; }
inline void decrement(atomic_count& a, long b) { a -= b; }
inline void ref_count_up(atomic_count& a) { ++a; }
inline bool ref_count_down(atomic_count& a) { return --a == 0; }
inline void ref_count_up_release(atomic_count& a) { ++a; }
inline long ref_count_read_acquire(atomic_count& a) { return a; }
#else // !defined(NETIO_HAS_THREADS)
typedef std::atomic<long> atomic_count;
inline void increment(atomic_count& a, long b) { a += b; }
inline void decrement(atomic_count& a, long b) { a -= b; }

inline void ref_count_up(atomic_count& a)
{
  a.fetch_add(1, std::memory_order_relaxed);
}

inline bool ref_count_down(atomic_count& a)
{
  if (a.fetch_sub(1, std::memory_order_release) == 1)
  {
    std::atomic_thread_fence(std::memory_order_acquire);
    return true;
  }
  return false;
}

inline void ref_count_up_release(atomic_count& a)
{
  a.fetch_add(1, std::memory_order_release);
}

inline long ref_count_read_acquire(atomic_count& a)
{
  return a.load(std::memory_order_acquire);
}

#endif // !defined(NETIO_HAS_THREADS)

} // namespace detail
} // namespace netio

#endif // NETIO_DETAIL_ATOMIC_COUNT_HPP
