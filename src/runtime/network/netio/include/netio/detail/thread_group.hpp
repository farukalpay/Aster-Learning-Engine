#ifndef NETIO_DETAIL_THREAD_GROUP_HPP
#define NETIO_DETAIL_THREAD_GROUP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/thread.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Allocator>
class thread_group
{
public:
  // Constructor initialises an empty thread group.
  explicit thread_group(const Allocator& a)
    : allocator_(a),
      first_(0)
  {
  }

  // Destructor joins any remaining threads in the group.
  ~thread_group()
  {
    join();
  }

  // Create a new thread in the group.
  template <typename Function>
  void create_thread(Function f)
  {
    first_ = allocate_object<item>(allocator_, allocator_, f, first_);
  }

  // Create new threads in the group.
  template <typename Function>
  void create_threads(Function f, std::size_t num_threads)
  {
    for (std::size_t i = 0; i < num_threads; ++i)
      create_thread(f);
  }

  // Wait for all threads in the group to exit.
  void join()
  {
    while (first_)
    {
      first_->thread_.join();
      item* tmp = first_;
      first_ = first_->next_;
      deallocate_object(allocator_, tmp);
    }
  }

  // Test whether the group is empty.
  bool empty() const
  {
    return first_ == 0;
  }

private:
  // Structure used to track a single thread in the group.
  struct item
  {
    template <typename Function>
    explicit item(const Allocator& a, Function f, item* next)
      : thread_(std::allocator_arg, a, f),
        next_(next)
    {
    }

    netio::detail::thread thread_;
    item* next_;
  };

  // The allocator to be used to create items in the group.
  Allocator allocator_;

  // The first thread in the group.
  item* first_;
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_THREAD_GROUP_HPP
