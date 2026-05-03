#ifndef NETIO_SSL_DETAIL_SHUTDOWN_OP_HPP
#define NETIO_SSL_DETAIL_SHUTDOWN_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/ssl/detail/engine.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {
namespace detail {

class shutdown_op
{
public:
  static constexpr const char* tracking_name()
  {
    return "ssl::stream<>::async_shutdown";
  }

  engine::want operator()(engine& eng,
      netio::error_code& ec,
      std::size_t& bytes_transferred) const
  {
    bytes_transferred = 0;
    return eng.shutdown(ec);
  }

  void complete_sync(netio::error_code& ec) const
  {
    if (ec == netio::error::eof)
    {
      // The engine only generates an eof when the shutdown notification has
      // been received from the peer. This indicates that the shutdown has
      // completed successfully, and thus need not be returned to the caller.
      ec = netio::error_code();
    }
  }

  template <typename Handler>
  void call_handler(Handler& handler,
      const netio::error_code& ec,
      const std::size_t&) const
  {
    if (ec == netio::error::eof)
    {
      // The engine only generates an eof when the shutdown notification has
      // been received from the peer. This indicates that the shutdown has
      // completed successfully, and thus need not be passed on to the handler.
      static_cast<Handler&&>(handler)(netio::error_code());
    }
    else
    {
      static_cast<Handler&&>(handler)(ec);
    }
  }
};

} // namespace detail
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_DETAIL_SHUTDOWN_OP_HPP
