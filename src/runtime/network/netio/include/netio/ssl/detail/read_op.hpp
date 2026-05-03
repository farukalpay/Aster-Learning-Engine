#ifndef NETIO_SSL_DETAIL_READ_OP_HPP
#define NETIO_SSL_DETAIL_READ_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/detail/buffer_sequence_adapter.hpp"
#include "netio/ssl/detail/engine.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {
namespace detail {

template <typename MutableBufferSequence>
class read_op
{
public:
  static constexpr const char* tracking_name()
  {
    return "ssl::stream<>::async_read_some";
  }

  read_op(const MutableBufferSequence& buffers)
    : buffers_(buffers)
  {
  }

  engine::want operator()(engine& eng,
      netio::error_code& ec,
      std::size_t& bytes_transferred) const
  {
    netio::mutable_buffer buffer =
      netio::detail::buffer_sequence_adapter<netio::mutable_buffer,
        MutableBufferSequence>::first(buffers_);

    return eng.read(buffer, ec, bytes_transferred);
  }

  void complete_sync(netio::error_code&) const
  {
  }

  template <typename Handler>
  void call_handler(Handler& handler,
      const netio::error_code& ec,
      const std::size_t& bytes_transferred) const
  {
    static_cast<Handler&&>(handler)(ec, bytes_transferred);
  }

private:
  MutableBufferSequence buffers_;
};

} // namespace detail
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_DETAIL_READ_OP_HPP
