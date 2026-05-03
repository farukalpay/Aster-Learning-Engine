#ifndef NETIO_SSL_DETAIL_HANDSHAKE_OP_HPP
#define NETIO_SSL_DETAIL_HANDSHAKE_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/ssl/detail/engine.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {
namespace detail {

class handshake_op
{
public:
  static constexpr const char* tracking_name()
  {
    return "ssl::stream<>::async_handshake";
  }

  handshake_op(stream_base::handshake_type type)
    : type_(type)
  {
  }

  engine::want operator()(engine& eng,
      netio::error_code& ec,
      std::size_t& bytes_transferred) const
  {
    bytes_transferred = 0;
    return eng.handshake(type_, ec);
  }

  void complete_sync(netio::error_code&) const
  {
  }

  template <typename Handler>
  void call_handler(Handler& handler,
      const netio::error_code& ec,
      const std::size_t&) const
  {
    static_cast<Handler&&>(handler)(ec);
  }

private:
  stream_base::handshake_type type_;
};

} // namespace detail
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_DETAIL_HANDSHAKE_OP_HPP
