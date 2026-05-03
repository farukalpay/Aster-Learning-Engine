#ifndef NETIO_REDIRECT_DISPOSITION_HPP
#define NETIO_REDIRECT_DISPOSITION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/detail/type_traits.hpp"
#include "netio/disposition.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

/// A @ref completion_token adapter used to specify that the disposition
/// produced by an asynchronous operation is captured to a variable.
/**
 * The redirect_disposition_t class is used to indicate that any disposition
 * produced by an asynchronous operation is captured to a specified variable.
 */
template <typename CompletionToken, NETIO_DISPOSITION Disposition>
class redirect_disposition_t
{
public:
  /// Constructor.
  template <typename T>
  redirect_disposition_t(T&& completion_token, Disposition& d)
    : token_(static_cast<T&&>(completion_token)),
      d_(d)
  {
  }

//private:
  CompletionToken token_;
  Disposition& d_;
};

/// A function object type that adapts a @ref completion_token to capture
/// any disposition produced by an asynchronous operation to a variable.
/**
 * May also be used directly as a completion token, in which case it adapts the
 * asynchronous operation's default completion token (or netio::deferred
 * if no default is available).
 */
template <NETIO_DISPOSITION Disposition>
class partial_redirect_disposition
{
public:
  /// Constructor that specifies the variable used to capture disposition
  /// values.
  explicit partial_redirect_disposition(Disposition& d)
    : d_(d)
  {
  }

  /// Adapt a @ref completion_token to specify that the completion handler
  /// should capture disposition values to a variable.
  template <typename CompletionToken>
  NETIO_NODISCARD inline
  constexpr redirect_disposition_t<decay_t<CompletionToken>, Disposition>
  operator()(CompletionToken&& completion_token) const
  {
    return redirect_disposition_t<decay_t<CompletionToken>, Disposition>(
        static_cast<CompletionToken&&>(completion_token), d_);
  }

//private:
  Disposition& d_;
};

/// Create a partial completion token adapter that captures disposition values
/// to a variable.
/**
 * @note When redirecting to a variable of type @c std::exception_ptr, other
 * disposition types will be automatically converted to @c std::exception_ptr.
 */
template <NETIO_DISPOSITION Disposition>
NETIO_NODISCARD inline partial_redirect_disposition<Disposition>
redirect_disposition(Disposition& d)
{
  return partial_redirect_disposition<Disposition>(d);
}

/// Adapt a @ref completion_token to capture disposition values to a variable.
/**
 * @note When redirecting to a variable of type @c std::exception_ptr, other
 * disposition types will be automatically converted to @c std::exception_ptr.
 */
template <typename CompletionToken, NETIO_DISPOSITION Disposition>
NETIO_NODISCARD inline
redirect_disposition_t<decay_t<CompletionToken>, Disposition>
redirect_disposition(CompletionToken&& completion_token, Disposition& d)
{
  return redirect_disposition_t<decay_t<CompletionToken>, Disposition>(
      static_cast<CompletionToken&&>(completion_token), d);
}

} // namespace netio

#include "netio/detail/pop_options.hpp"

#include "netio/impl/redirect_disposition.hpp"

#endif // NETIO_REDIRECT_DISPOSITION_HPP
