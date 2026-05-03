#ifndef NETIO_SSL_DETAIL_STREAM_CORE_HPP
#define NETIO_SSL_DETAIL_STREAM_CORE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#include "netio/ssl/detail/engine.hpp"
#include "netio/buffer.hpp"
#include "netio/steady_timer.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace ssl {
namespace detail {

struct stream_core
{
  // According to the OpenSSL documentation, this is the buffer size that is
  // sufficient to hold the largest possible TLS record.
  enum { max_tls_record_size = 17 * 1024 };

  template <typename Executor>
  stream_core(SSL_CTX* context, const Executor& ex)
    : engine_(context),
      pending_read_(ex),
      pending_write_(ex),
      output_buffer_space_(max_tls_record_size),
      output_buffer_(netio::buffer(output_buffer_space_)),
      input_buffer_space_(max_tls_record_size),
      input_buffer_(netio::buffer(input_buffer_space_))
  {
    pending_read_.expires_at(neg_infin());
    pending_write_.expires_at(neg_infin());
  }

  template <typename Executor>
  stream_core(SSL* ssl_impl, const Executor& ex)
    : engine_(ssl_impl),
      pending_read_(ex),
      pending_write_(ex),
      output_buffer_space_(max_tls_record_size),
      output_buffer_(netio::buffer(output_buffer_space_)),
      input_buffer_space_(max_tls_record_size),
      input_buffer_(netio::buffer(input_buffer_space_))
  {
    pending_read_.expires_at(neg_infin());
    pending_write_.expires_at(neg_infin());
  }

  stream_core(stream_core&& other)
    : engine_(static_cast<engine&&>(other.engine_)),
      pending_read_(
         static_cast<netio::steady_timer&&>(
           other.pending_read_)),
      pending_write_(
         static_cast<netio::steady_timer&&>(
           other.pending_write_)),
      output_buffer_space_(
          static_cast<std::vector<unsigned char>&&>(
            other.output_buffer_space_)),
      output_buffer_(other.output_buffer_),
      input_buffer_space_(
          static_cast<std::vector<unsigned char>&&>(
            other.input_buffer_space_)),
      input_buffer_(other.input_buffer_),
      input_(other.input_)
  {
    other.output_buffer_ = netio::mutable_buffer(0, 0);
    other.input_buffer_ = netio::mutable_buffer(0, 0);
    other.input_ = netio::const_buffer(0, 0);
  }

  ~stream_core()
  {
  }

  stream_core& operator=(stream_core&& other)
  {
    if (this != &other)
    {
      engine_ = static_cast<engine&&>(other.engine_);
      pending_read_ =
        static_cast<netio::steady_timer&&>(
          other.pending_read_);
      pending_write_ =
        static_cast<netio::steady_timer&&>(
          other.pending_write_);
      output_buffer_space_ =
        static_cast<std::vector<unsigned char>&&>(
          other.output_buffer_space_);
      output_buffer_ = other.output_buffer_;
      input_buffer_space_ =
        static_cast<std::vector<unsigned char>&&>(
          other.input_buffer_space_);
      input_buffer_ = other.input_buffer_;
      input_ = other.input_;
      other.output_buffer_ = netio::mutable_buffer(0, 0);
      other.input_buffer_ = netio::mutable_buffer(0, 0);
      other.input_ = netio::const_buffer(0, 0);
    }
    return *this;
  }

  // The SSL engine.
  engine engine_;

  // Timer used for storing queued read operations.
  netio::steady_timer pending_read_;

  // Timer used for storing queued write operations.
  netio::steady_timer pending_write_;

  // Helper function for obtaining a time value that always fires.
  static netio::steady_timer::time_point neg_infin()
  {
    return (netio::steady_timer::time_point::min)();
  }

  // Helper function for obtaining a time value that never fires.
  static netio::steady_timer::time_point pos_infin()
  {
    return (netio::steady_timer::time_point::max)();
  }

  // Helper function to get a timer's expiry time.
  static netio::steady_timer::time_point expiry(
      const netio::steady_timer& timer)
  {
    return timer.expiry();
  }

  // Buffer space used to prepare output intended for the transport.
  std::vector<unsigned char> output_buffer_space_;

  // A buffer that may be used to prepare output intended for the transport.
  netio::mutable_buffer output_buffer_;

  // Buffer space used to read input intended for the engine.
  std::vector<unsigned char> input_buffer_space_;

  // A buffer that may be used to read input intended for the engine.
  netio::mutable_buffer input_buffer_;

  // The buffer pointing to the engine's unconsumed input.
  netio::const_buffer input_;
};

} // namespace detail
} // namespace ssl
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // NETIO_SSL_DETAIL_STREAM_CORE_HPP
