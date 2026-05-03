#ifndef NETIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP
#define NETIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/recycling_allocator.hpp"
#include "netio/associated_allocator.hpp"

#include "netio/detail/push_options.hpp"

#define NETIO_DEFINE_TAGGED_HANDLER_PTR(purpose, op) \
  struct ptr \
  { \
    Handler* h; \
    op* v; \
    op* p; \
    ~ptr() \
    { \
      reset(); \
    } \
    static op* allocate(Handler& handler) \
    { \
      typedef typename ::netio::associated_allocator< \
        Handler>::type associated_allocator_type; \
      typedef typename ::netio::detail::get_recycling_allocator< \
        associated_allocator_type, purpose>::type default_allocator_type; \
      NETIO_REBIND_ALLOC(default_allocator_type, op) a( \
            ::netio::detail::get_recycling_allocator< \
              associated_allocator_type, purpose>::get( \
                ::netio::get_associated_allocator(handler))); \
      return a.allocate(1); \
    } \
    void reset() \
    { \
      if (p) \
      { \
        p->~op(); \
        p = 0; \
      } \
      if (v) \
      { \
        typedef typename ::netio::associated_allocator< \
          Handler>::type associated_allocator_type; \
        typedef typename ::netio::detail::get_recycling_allocator< \
          associated_allocator_type, purpose>::type default_allocator_type; \
        NETIO_REBIND_ALLOC(default_allocator_type, op) a( \
              ::netio::detail::get_recycling_allocator< \
                associated_allocator_type, purpose>::get( \
                  ::netio::get_associated_allocator(*h))); \
        a.deallocate(static_cast<op*>(v), 1); \
        v = 0; \
      } \
    } \
  } \
  /**/

#define NETIO_DEFINE_HANDLER_PTR(op) \
  NETIO_DEFINE_TAGGED_HANDLER_PTR( \
      ::netio::detail::thread_info_base::default_tag, op ) \
  /**/

#define NETIO_DEFINE_TAGGED_HANDLER_ALLOCATOR_PTR(purpose, op) \
  struct ptr \
  { \
    const Alloc* a; \
    void* v; \
    op* p; \
    ~ptr() \
    { \
      reset(); \
    } \
    static op* allocate(const Alloc& a) \
    { \
      typedef typename ::netio::detail::get_recycling_allocator< \
        Alloc, purpose>::type recycling_allocator_type; \
      NETIO_REBIND_ALLOC(recycling_allocator_type, op) a1( \
            ::netio::detail::get_recycling_allocator< \
              Alloc, purpose>::get(a)); \
      return a1.allocate(1); \
    } \
    void reset() \
    { \
      if (p) \
      { \
        p->~op(); \
        p = 0; \
      } \
      if (v) \
      { \
        typedef typename ::netio::detail::get_recycling_allocator< \
          Alloc, purpose>::type recycling_allocator_type; \
        NETIO_REBIND_ALLOC(recycling_allocator_type, op) a1( \
              ::netio::detail::get_recycling_allocator< \
                Alloc, purpose>::get(*a)); \
        a1.deallocate(static_cast<op*>(v), 1); \
        v = 0; \
      } \
    } \
  } \
  /**/

#define NETIO_DEFINE_HANDLER_ALLOCATOR_PTR(op) \
  NETIO_DEFINE_TAGGED_HANDLER_ALLOCATOR_PTR( \
      ::netio::detail::thread_info_base::default_tag, op ) \
  /**/

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP
