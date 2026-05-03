#ifndef NETIO_DETAIL_BASE_FROM_CANCELLATION_STATE_HPP
#define NETIO_DETAIL_BASE_FROM_CANCELLATION_STATE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/associated_cancellation_slot.hpp"
#include "netio/cancellation_state.hpp"
#include "netio/detail/type_traits.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Handler, typename = void>
class base_from_cancellation_state
{
public:
  typedef cancellation_slot cancellation_slot_type;

  cancellation_slot_type get_cancellation_slot() const noexcept
  {
    return cancellation_state_.slot();
  }

  cancellation_state get_cancellation_state() const noexcept
  {
    return cancellation_state_;
  }

protected:
  explicit base_from_cancellation_state(const Handler& handler)
    : cancellation_state_(
        netio::get_associated_cancellation_slot(handler))
  {
  }

  template <typename Filter>
  base_from_cancellation_state(const Handler& handler, Filter filter)
    : cancellation_state_(
        netio::get_associated_cancellation_slot(handler), filter, filter)
  {
  }

  template <typename InFilter, typename OutFilter>
  base_from_cancellation_state(const Handler& handler,
      InFilter&& in_filter,
      OutFilter&& out_filter)
    : cancellation_state_(
        netio::get_associated_cancellation_slot(handler),
        static_cast<InFilter&&>(in_filter),
        static_cast<OutFilter&&>(out_filter))
  {
  }

  void reset_cancellation_state(const Handler& handler)
  {
    cancellation_state_ = cancellation_state(
        netio::get_associated_cancellation_slot(handler));
  }

  template <typename Filter>
  void reset_cancellation_state(const Handler& handler, Filter filter)
  {
    cancellation_state_ = cancellation_state(
        netio::get_associated_cancellation_slot(handler), filter, filter);
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(const Handler& handler,
      InFilter&& in_filter,
      OutFilter&& out_filter)
  {
    cancellation_state_ = cancellation_state(
        netio::get_associated_cancellation_slot(handler),
        static_cast<InFilter&&>(in_filter),
        static_cast<OutFilter&&>(out_filter));
  }

  cancellation_type_t cancelled() const noexcept
  {
    return cancellation_state_.cancelled();
  }

private:
  cancellation_state cancellation_state_;
};

template <typename Handler>
class base_from_cancellation_state<Handler,
    enable_if_t<
      is_same<
        typename associated_cancellation_slot<
          Handler, cancellation_slot
        >::netio_associated_cancellation_slot_is_unspecialised,
        void
      >::value
    >
  >
{
public:
  cancellation_state get_cancellation_state() const noexcept
  {
    return cancellation_state();
  }

protected:
  explicit base_from_cancellation_state(const Handler&)
  {
  }

  template <typename Filter>
  base_from_cancellation_state(const Handler&, Filter)
  {
  }

  template <typename InFilter, typename OutFilter>
  base_from_cancellation_state(const Handler&,
      InFilter&&,
      OutFilter&&)
  {
  }

  void reset_cancellation_state(const Handler&)
  {
  }

  template <typename Filter>
  void reset_cancellation_state(const Handler&, Filter)
  {
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(const Handler&,
      InFilter&&,
      OutFilter&&)
  {
  }

  constexpr cancellation_type_t cancelled() const noexcept
  {
    return cancellation_type::none;
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_DETAIL_BASE_FROM_CANCELLATION_STATE_HPP
