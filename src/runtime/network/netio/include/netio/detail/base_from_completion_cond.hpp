#ifndef NETIO_DETAIL_BASE_FROM_COMPLETION_COND_HPP
#define NETIO_DETAIL_BASE_FROM_COMPLETION_COND_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/completion_condition.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename CompletionCondition>
class base_from_completion_cond
{
protected:
  explicit base_from_completion_cond(CompletionCondition& completion_condition)
    : completion_condition_(
        static_cast<CompletionCondition&&>(completion_condition))
  {
  }

  std::size_t check_for_completion(
      const netio::error_code& ec,
      std::size_t total_transferred)
  {
    return detail::adapt_completion_condition_result(
        completion_condition_(ec, total_transferred));
  }

private:
  CompletionCondition completion_condition_;
};

template <>
class base_from_completion_cond<transfer_all_t>
{
protected:
  explicit base_from_completion_cond(transfer_all_t)
  {
  }

  static std::size_t check_for_completion(
      const netio::error_code& ec,
      std::size_t total_transferred)
  {
    return transfer_all_t()(ec, total_transferred);
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_BASE_FROM_COMPLETION_COND_HPP
