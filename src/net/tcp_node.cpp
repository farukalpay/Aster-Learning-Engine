// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/tcp_node.hpp"

#if ASTER_HAS_NETWORK_TRANSPORT

#include <netio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace aster::net {
namespace {

using Tcp = netio::ip::tcp;

} // namespace

class TcpSession;

struct TcpNode::Impl {
  explicit Impl(NodeProfile node_profile)
      : profile(std::move(node_profile)), acceptor(io), work_guard(netio::make_work_guard(io)) {}

  void doAccept();
  void addSession(const std::shared_ptr<TcpSession> &session);
  void forgetSession(const TcpSession *session);
  void receive(const NetMessage &message);
  void closeAllSessions();

  NodeProfile profile;
  NodeRouter router;
  netio::io_context io;
  Tcp::acceptor acceptor;
  netio::executor_work_guard<netio::io_context::executor_type> work_guard;
  std::vector<std::shared_ptr<TcpSession>> sessions;
  mutable std::mutex session_mutex;
  std::thread io_thread;
  std::atomic_bool is_running = false;
};

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
  TcpSession(Tcp::socket socket, TcpNode::Impl &owner)
      : socket_(std::move(socket)), owner_(owner) {}

  Tcp::socket &socket() {
    return socket_;
  }

  void start() {
    readPrefix();
  }

  void send(NetMessage message) {
    std::vector<std::uint8_t> frame = encodeFrame(message);
    netio::post(socket_.get_executor(),
                [self = shared_from_this(), frame = std::move(frame)]() mutable {
                  const bool write_in_progress = !self->write_queue_.empty();
                  self->write_queue_.push_back(std::move(frame));
                  if (!write_in_progress) {
                    self->writeNext();
                  }
                });
  }

  void close() {
    closeSocket();
    owner_.forgetSession(this);
  }

  void closeSocket() {
    netio::error_code ignored;
    socket_.shutdown(Tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
  }

private:
  void readPrefix() {
    auto self = shared_from_this();
    netio::async_read(socket_, netio::buffer(prefix_),
                      [this, self](const netio::error_code &error, std::size_t) {
                        if (error) {
                          close();
                          return;
                        }

                        std::uint32_t body_bytes = 0;
                        const FrameDecodeResult status = inspectFramePrefix(prefix_, body_bytes);
                        if (status != FrameDecodeResult::Ready) {
                          close();
                          return;
                        }

                        body_.assign(body_bytes, 0);
                        readBody();
                      });
  }

  void readBody() {
    auto self = shared_from_this();
    netio::async_read(socket_, netio::buffer(body_),
                      [this, self](const netio::error_code &error, std::size_t) {
                        if (error) {
                          close();
                          return;
                        }

                        std::vector<std::uint8_t> frame;
                        frame.reserve(prefix_.size() + body_.size());
                        frame.insert(frame.end(), prefix_.begin(), prefix_.end());
                        frame.insert(frame.end(), body_.begin(), body_.end());

                        NetMessage message;
                        if (decodeNextFrame(frame, message) != FrameDecodeResult::Ready) {
                          close();
                          return;
                        }

                        owner_.receive(message);
                        readPrefix();
                      });
  }

  void writeNext() {
    auto self = shared_from_this();
    netio::async_write(socket_, netio::buffer(write_queue_.front()),
                       [this, self](const netio::error_code &error, std::size_t) {
                         if (error) {
                           close();
                           return;
                         }

                         write_queue_.pop_front();
                         if (!write_queue_.empty()) {
                           writeNext();
                         }
                       });
  }

  Tcp::socket socket_;
  TcpNode::Impl &owner_;
  std::array<std::uint8_t, kFramePrefixBytes> prefix_{};
  std::vector<std::uint8_t> body_;
  std::deque<std::vector<std::uint8_t>> write_queue_;
};

void TcpNode::Impl::doAccept() {
  acceptor.async_accept([this](const netio::error_code &error, Tcp::socket socket) {
    if (!error) {
      auto session = std::make_shared<TcpSession>(std::move(socket), *this);
      addSession(session);
      session->start();
    }

    if (acceptor.is_open()) {
      doAccept();
    }
  });
}

void TcpNode::Impl::addSession(const std::shared_ptr<TcpSession> &session) {
  std::lock_guard lock(session_mutex);
  sessions.push_back(session);
}

void TcpNode::Impl::forgetSession(const TcpSession *session) {
  std::lock_guard lock(session_mutex);
  sessions.erase(std::remove_if(sessions.begin(), sessions.end(),
                                [session](const std::shared_ptr<TcpSession> &entry) {
                                  return entry.get() == session;
                                }),
                 sessions.end());
}

void TcpNode::Impl::receive(const NetMessage &message) {
  router.route(message);
}

void TcpNode::Impl::closeAllSessions() {
  std::vector<std::shared_ptr<TcpSession>> local_sessions;
  {
    std::lock_guard lock(session_mutex);
    local_sessions.swap(sessions);
  }

  for (const std::shared_ptr<TcpSession> &session : local_sessions) {
    session->closeSocket();
  }
}

TcpNode::TcpNode(NodeProfile profile) : impl_(std::make_unique<Impl>(std::move(profile))) {}

TcpNode::~TcpNode() {
  stop();
}

NodeRouter &TcpNode::router() {
  return impl_->router;
}

const NodeRouter &TcpNode::router() const {
  return impl_->router;
}

const NodeProfile &TcpNode::profile() const {
  return impl_->profile;
}

std::uint16_t TcpNode::listen(const std::uint16_t port) {
  Tcp::endpoint endpoint(Tcp::v4(), port);
  impl_->acceptor.open(endpoint.protocol());
  impl_->acceptor.set_option(Tcp::acceptor::reuse_address(true));
  impl_->acceptor.bind(endpoint);
  impl_->acceptor.listen();
  impl_->doAccept();
  return impl_->acceptor.local_endpoint().port();
}

void TcpNode::connect(const std::string &host, const std::uint16_t port) {
  Tcp::resolver resolver(impl_->io);
  auto endpoints = resolver.resolve(host, std::to_string(port));
  auto session = std::make_shared<TcpSession>(Tcp::socket(impl_->io), *impl_);

  netio::async_connect(
      session->socket(), endpoints,
      [owner = impl_.get(), session](const netio::error_code &error, const Tcp::endpoint &) {
        if (!error) {
          owner->addSession(session);
          session->start();
        }
      });
}

void TcpNode::runAsync() {
  bool expected = false;
  if (!impl_->is_running.compare_exchange_strong(expected, true)) {
    return;
  }

  impl_->io_thread = std::thread([impl = impl_.get()] {
    impl->io.run();
    impl->is_running = false;
  });
}

void TcpNode::stop() {
  if (!impl_) {
    return;
  }

  if (impl_->io_thread.joinable()) {
    netio::post(impl_->io, [impl = impl_.get()] {
      netio::error_code ignored;
      impl->acceptor.close(ignored);
      impl->closeAllSessions();
      impl->work_guard.reset();
    });

    if (impl_->io_thread.get_id() != std::this_thread::get_id()) {
      impl_->io_thread.join();
    }
  } else {
    netio::error_code ignored;
    impl_->acceptor.close(ignored);
    impl_->closeAllSessions();
    impl_->work_guard.reset();
    impl_->io.stop();
  }

  impl_->is_running = false;
}

void TcpNode::send(NetMessage message) {
  if (message.source_node == kUnassignedNodeId) {
    message.source_node = impl_->profile.id;
  }

  std::vector<std::shared_ptr<TcpSession>> local_sessions;
  {
    std::lock_guard lock(impl_->session_mutex);
    local_sessions = impl_->sessions;
  }

  for (const std::shared_ptr<TcpSession> &session : local_sessions) {
    session->send(message);
  }
}

bool TcpNode::running() const {
  return impl_->is_running;
}

std::size_t TcpNode::peerCount() const {
  std::lock_guard lock(impl_->session_mutex);
  return impl_->sessions.size();
}

} // namespace aster::net

#endif
