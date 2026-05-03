#ifndef NETIO_IS_READ_BUFFERED_HPP
#define NETIO_IS_READ_BUFFERED_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"
#include "netio/buffered_read_stream_fwd.hpp"
#include "netio/buffered_stream_fwd.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {

namespace detail {

template <typename Stream>
char is_read_buffered_helper(buffered_stream<Stream>* s);

template <typename Stream>
char is_read_buffered_helper(buffered_read_stream<Stream>* s);

struct is_read_buffered_big_type { char data[10]; };
is_read_buffered_big_type is_read_buffered_helper(...);

} // namespace detail

/// The is_read_buffered class is a traits class that may be used to determine
/// whether a stream type supports buffering of read data.
template <typename Stream>
class is_read_buffered
{
public:
#if defined(GENERATING_DOCUMENTATION)
  /// The value member is true only if the Stream type supports buffering of
  /// read data.
  static const bool value;
#else
  NETIO_STATIC_CONSTANT(bool,
      value = sizeof(detail::is_read_buffered_helper((Stream*)0)) == 1);
#endif
};

} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_IS_READ_BUFFERED_HPP
