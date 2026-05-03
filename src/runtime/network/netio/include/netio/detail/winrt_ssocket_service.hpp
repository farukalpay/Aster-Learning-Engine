#ifndef NETIO_DETAIL_WINRT_SSOCKET_SERVICE_HPP
#define NETIO_DETAIL_WINRT_SSOCKET_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "netio/detail/config.hpp"

#if defined(NETIO_WINDOWS_RUNTIME)

#include "netio/error.hpp"
#include "netio/execution_context.hpp"
#include "netio/detail/memory.hpp"
#include "netio/detail/winrt_socket_connect_op.hpp"
#include "netio/detail/winrt_ssocket_service_base.hpp"
#include "netio/detail/winrt_utils.hpp"

#include "netio/detail/push_options.hpp"

namespace netio {
namespace detail {

template <typename Protocol>
class winrt_ssocket_service :
  public execution_context_service_base<winrt_ssocket_service<Protocol>>,
  public winrt_ssocket_service_base
{
public:
  // The protocol type.
  typedef Protocol protocol_type;

  // The endpoint type.
  typedef typename Protocol::endpoint endpoint_type;

  // The native type of a socket.
  typedef Windows::Networking::Sockets::StreamSocket^ native_handle_type;

  // The implementation type of the socket.
  struct implementation_type : base_implementation_type
  {
    // Default constructor.
    implementation_type()
      : base_implementation_type(),
        protocol_(endpoint_type().protocol())
    {
    }

    // The protocol associated with the socket.
    protocol_type protocol_;
  };

  // Constructor.
  winrt_ssocket_service(execution_context& context)
    : execution_context_service_base<winrt_ssocket_service<Protocol>>(context),
      winrt_ssocket_service_base(context)
  {
  }

  // Destroy all user-defined handler objects owned by the service.
  void shutdown()
  {
    this->base_shutdown();
  }

  // Move-construct a new socket implementation.
  void move_construct(implementation_type& impl,
      implementation_type& other_impl) noexcept
  {
    this->base_move_construct(impl, other_impl);

    impl.protocol_ = other_impl.protocol_;
    other_impl.protocol_ = endpoint_type().protocol();
  }

  // Move-assign from another socket implementation.
  void move_assign(implementation_type& impl,
      winrt_ssocket_service& other_service,
      implementation_type& other_impl)
  {
    this->base_move_assign(impl, other_service, other_impl);

    impl.protocol_ = other_impl.protocol_;
    other_impl.protocol_ = endpoint_type().protocol();
  }

  // Move-construct a new socket implementation from another protocol type.
  template <typename Protocol1>
  void converting_move_construct(implementation_type& impl,
      winrt_ssocket_service<Protocol1>&,
      typename winrt_ssocket_service<
        Protocol1>::implementation_type& other_impl)
  {
    this->base_move_construct(impl, other_impl);

    impl.protocol_ = protocol_type(other_impl.protocol_);
    other_impl.protocol_ = typename Protocol1::endpoint().protocol();
  }

  // Open a new socket implementation.
  netio::error_code open(implementation_type& impl,
      const protocol_type& protocol, netio::error_code& ec)
  {
    if (is_open(impl))
    {
      ec = netio::error::already_open;
      return ec;
    }

    try
    {
      impl.socket_ = ref new Windows::Networking::Sockets::StreamSocket;
      impl.protocol_ = protocol;
      ec = netio::error_code();
    }
    catch (Platform::Exception^ e)
    {
      ec = netio::error_code(e->HResult,
            netio::system_category());
    }

    return ec;
  }

  // Assign a native socket to a socket implementation.
  netio::error_code assign(implementation_type& impl,
      const protocol_type& protocol, const native_handle_type& native_socket,
      netio::error_code& ec)
  {
    if (is_open(impl))
    {
      ec = netio::error::already_open;
      return ec;
    }

    impl.socket_ = native_socket;
    impl.protocol_ = protocol;
    ec = netio::error_code();

    return ec;
  }

  // Bind the socket to the specified local endpoint.
  netio::error_code bind(implementation_type&,
      const endpoint_type&, netio::error_code& ec)
  {
    ec = netio::error::operation_not_supported;
    return ec;
  }

  // Get the local endpoint.
  endpoint_type local_endpoint(const implementation_type& impl,
      netio::error_code& ec) const
  {
    endpoint_type endpoint;
    endpoint.resize(do_get_endpoint(impl, true,
          endpoint.data(), endpoint.size(), ec));
    return endpoint;
  }

  // Get the remote endpoint.
  endpoint_type remote_endpoint(const implementation_type& impl,
      netio::error_code& ec) const
  {
    endpoint_type endpoint;
    endpoint.resize(do_get_endpoint(impl, false,
          endpoint.data(), endpoint.size(), ec));
    return endpoint;
  }

  // Disable sends or receives on the socket.
  netio::error_code shutdown(implementation_type&,
      socket_base::shutdown_type, netio::error_code& ec)
  {
    ec = netio::error::operation_not_supported;
    return ec;
  }

  // Set a socket option.
  template <typename Option>
  netio::error_code set_option(implementation_type& impl,
      const Option& option, netio::error_code& ec)
  {
    return do_set_option(impl, option.level(impl.protocol_),
        option.name(impl.protocol_), option.data(impl.protocol_),
        option.size(impl.protocol_), ec);
  }

  // Get a socket option.
  template <typename Option>
  netio::error_code get_option(const implementation_type& impl,
      Option& option, netio::error_code& ec) const
  {
    std::size_t size = option.size(impl.protocol_);
    do_get_option(impl, option.level(impl.protocol_),
        option.name(impl.protocol_),
        option.data(impl.protocol_), &size, ec);
    if (!ec)
      option.resize(impl.protocol_, size);
    return ec;
  }

  // Connect the socket to the specified endpoint.
  netio::error_code connect(implementation_type& impl,
      const endpoint_type& peer_endpoint, netio::error_code& ec)
  {
    return do_connect(impl, peer_endpoint.data(), ec);
  }

  // Start an asynchronous connect.
  template <typename Handler, typename IoExecutor>
  void async_connect(implementation_type& impl,
      const endpoint_type& peer_endpoint,
      Handler& handler, const IoExecutor& io_ex)
  {
    bool is_continuation =
      netio_handler_cont_helpers::is_continuation(handler);

    // Allocate and construct an operation to wrap the handler.
    typedef winrt_socket_connect_op<Handler, IoExecutor> op;
    typename op::ptr p = { netio::detail::addressof(handler),
      op::ptr::allocate(handler), 0 };
    p.p = new (p.v) op(handler, io_ex);

    NETIO_HANDLER_CREATION((scheduler_.context(),
          *p.p, "socket", &impl, 0, "async_connect"));

    start_connect_op(impl, peer_endpoint.data(), p.p, is_continuation);
    p.v = p.p = 0;
  }
};

} // namespace detail
} // namespace netio

#include "netio/detail/pop_options.hpp"

#endif // defined(NETIO_WINDOWS_RUNTIME)

#endif // NETIO_DETAIL_WINRT_SSOCKET_SERVICE_HPP
