// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/tcp_node.hpp"

#if ASTER_HAS_NETWORK_TRANSPORT

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace aster::net {
namespace {

constexpr int kPollTimeoutMs = 16;
constexpr std::size_t kSocketReadChunkBytes = 4096;

void closeFd(int &fd) {
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

void closeSocket(int &fd) {
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    closeFd(fd);
  }
}

void configureSocket(const int fd) {
  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_NOSIGPIPE
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif

  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

bool wouldBlock() {
  return errno == EAGAIN || errno == EWOULDBLOCK;
}

} // namespace

struct TcpSession {
  explicit TcpSession(const int socket_fd) : fd(socket_fd) {}

  int fd = -1;
  bool closing = false;
  std::vector<std::uint8_t> input;
  std::deque<std::vector<std::uint8_t>> output;
  std::size_t output_offset = 0;
};

struct TcpNode::Impl {
  explicit Impl(NodeProfile node_profile) : profile(std::move(node_profile)) {}

  void addSessionLocked(int fd);
  void acceptAvailableLocked();
  void readAvailableLocked(TcpSession &session, std::vector<NetMessage> &received);
  void writeAvailableLocked(TcpSession &session);
  void cleanupSessionsLocked();
  void eventLoop();
  void closeAllSockets();

  NodeProfile profile;
  NodeRouter router;
  int listen_fd = -1;
  std::vector<std::shared_ptr<TcpSession>> sessions;
  mutable std::mutex mutex;
  std::thread worker;
  std::atomic_bool running = false;
};

void TcpNode::Impl::addSessionLocked(const int fd) {
  configureSocket(fd);
  sessions.push_back(std::make_shared<TcpSession>(fd));
}

void TcpNode::Impl::acceptAvailableLocked() {
  if (listen_fd < 0) {
    return;
  }
  for (;;) {
    sockaddr_storage address{};
    socklen_t address_size = sizeof(address);
    const int fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&address), &address_size);
    if (fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    addSessionLocked(fd);
  }
}

void TcpNode::Impl::readAvailableLocked(TcpSession &session, std::vector<NetMessage> &received) {
  std::array<std::uint8_t, kSocketReadChunkBytes> chunk{};
  for (;;) {
    const ssize_t read_bytes = recv(session.fd, chunk.data(), chunk.size(), 0);
    if (read_bytes > 0) {
      session.input.insert(session.input.end(), chunk.begin(),
                           chunk.begin() + static_cast<std::ptrdiff_t>(read_bytes));
      continue;
    }
    if (read_bytes == 0) {
      session.closing = true;
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    if (!wouldBlock()) {
      session.closing = true;
    }
    break;
  }

  while (!session.closing) {
    NetMessage message;
    const FrameDecodeResult result = decodeNextFrame(session.input, message);
    if (result == FrameDecodeResult::Ready) {
      received.push_back(std::move(message));
      continue;
    }
    if (result == FrameDecodeResult::Incomplete) {
      break;
    }
    session.closing = true;
  }
}

void TcpNode::Impl::writeAvailableLocked(TcpSession &session) {
  while (!session.closing && !session.output.empty()) {
    const std::vector<std::uint8_t> &frame = session.output.front();
    const std::size_t remaining = frame.size() - session.output_offset;
    const ssize_t written =
        ::send(session.fd, frame.data() + session.output_offset, remaining,
#ifdef MSG_NOSIGNAL
             MSG_NOSIGNAL
#else
             0
#endif
        );
    if (written > 0) {
      session.output_offset += static_cast<std::size_t>(written);
      if (session.output_offset >= frame.size()) {
        session.output.pop_front();
        session.output_offset = 0;
      }
      continue;
    }
    if (written == 0) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    if (!wouldBlock()) {
      session.closing = true;
    }
    break;
  }
}

void TcpNode::Impl::cleanupSessionsLocked() {
  for (const std::shared_ptr<TcpSession> &session : sessions) {
    if (session->closing) {
      closeSocket(session->fd);
    }
  }
  sessions.erase(std::remove_if(sessions.begin(), sessions.end(),
                                [](const std::shared_ptr<TcpSession> &session) {
                                  return session->closing || session->fd < 0;
                                }),
                 sessions.end());
}

void TcpNode::Impl::eventLoop() {
  while (running) {
    std::vector<std::shared_ptr<TcpSession>> session_snapshot;
    std::vector<pollfd> poll_fds;
    bool has_listener = false;
    {
      std::lock_guard lock(mutex);
      cleanupSessionsLocked();
      if (listen_fd >= 0) {
        poll_fds.push_back({listen_fd, POLLIN, 0});
        has_listener = true;
      }
      session_snapshot = sessions;
      for (const std::shared_ptr<TcpSession> &session : session_snapshot) {
        short events = POLLIN | POLLERR | POLLHUP;
        if (!session->output.empty()) {
          events |= POLLOUT;
        }
        poll_fds.push_back({session->fd, events, 0});
      }
    }

    if (poll_fds.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kPollTimeoutMs));
      continue;
    }

    const int poll_result = poll(poll_fds.data(), poll_fds.size(), kPollTimeoutMs);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      running = false;
      break;
    }
    if (poll_result == 0) {
      continue;
    }

    std::vector<NetMessage> received;
    {
      std::lock_guard lock(mutex);
      std::size_t poll_index = 0;
      if (has_listener) {
        if ((poll_fds[poll_index].revents & POLLIN) != 0) {
          acceptAvailableLocked();
        }
        if ((poll_fds[poll_index].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
          closeFd(listen_fd);
        }
        ++poll_index;
      }

      for (const std::shared_ptr<TcpSession> &session : session_snapshot) {
        if (session == nullptr || session->fd < 0) {
          ++poll_index;
          continue;
        }
        const short revents = poll_fds[poll_index].revents;
        if ((revents & POLLIN) != 0) {
          readAvailableLocked(*session, received);
        }
        if ((revents & POLLOUT) != 0) {
          writeAvailableLocked(*session);
        }
        if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
          session->closing = true;
        }
        ++poll_index;
      }
      cleanupSessionsLocked();
    }

    for (const NetMessage &message : received) {
      router.route(message);
    }
  }
}

void TcpNode::Impl::closeAllSockets() {
  std::lock_guard lock(mutex);
  closeFd(listen_fd);
  for (const std::shared_ptr<TcpSession> &session : sessions) {
    if (session != nullptr) {
      closeSocket(session->fd);
      session->closing = true;
    }
  }
  sessions.clear();
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
  std::lock_guard lock(impl_->mutex);
  if (impl_->listen_fd >= 0) {
    throw std::runtime_error("TCP node is already listening.");
  }

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("Could not create TCP socket.");
  }
  configureSocket(fd);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    closeFd(fd);
    throw std::runtime_error("Could not bind TCP socket.");
  }
  if (::listen(fd, 16) != 0) {
    closeFd(fd);
    throw std::runtime_error("Could not listen on TCP socket.");
  }

  sockaddr_in actual{};
  socklen_t actual_size = sizeof(actual);
  if (getsockname(fd, reinterpret_cast<sockaddr *>(&actual), &actual_size) != 0) {
    closeFd(fd);
    throw std::runtime_error("Could not inspect TCP socket port.");
  }
  impl_->listen_fd = fd;
  return ntohs(actual.sin_port);
}

void TcpNode::connect(const std::string &host, const std::uint16_t port) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *result = nullptr;
  const std::string service = std::to_string(port);
  if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0) {
    throw std::runtime_error("Could not resolve TCP host.");
  }
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(result, freeaddrinfo);

  int fd = -1;
  for (addrinfo *entry = addresses.get(); entry != nullptr; entry = entry->ai_next) {
    fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
    if (fd < 0) {
      continue;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
    if (::connect(fd, entry->ai_addr, entry->ai_addrlen) == 0) {
      break;
    }
    closeFd(fd);
  }
  if (fd < 0) {
    throw std::runtime_error("Could not connect TCP socket.");
  }

  std::lock_guard lock(impl_->mutex);
  impl_->addSessionLocked(fd);
}

void TcpNode::runAsync() {
  bool expected = false;
  if (!impl_->running.compare_exchange_strong(expected, true)) {
    return;
  }
  impl_->worker = std::thread([impl = impl_.get()] { impl->eventLoop(); });
}

void TcpNode::stop() {
  if (!impl_) {
    return;
  }
  impl_->running = false;
  {
    std::lock_guard lock(impl_->mutex);
    closeFd(impl_->listen_fd);
  }
  if (impl_->worker.joinable() && impl_->worker.get_id() != std::this_thread::get_id()) {
    impl_->worker.join();
  }
  impl_->closeAllSockets();
}

void TcpNode::send(NetMessage message) {
  if (message.source_node == kUnassignedNodeId) {
    message.source_node = impl_->profile.id;
  }
  std::vector<std::uint8_t> frame = encodeFrame(message);
  std::lock_guard lock(impl_->mutex);
  for (const std::shared_ptr<TcpSession> &session : impl_->sessions) {
    if (session != nullptr && !session->closing) {
      session->output.push_back(frame);
    }
  }
}

bool TcpNode::running() const {
  return impl_->running;
}

std::size_t TcpNode::peerCount() const {
  std::lock_guard lock(impl_->mutex);
  return static_cast<std::size_t>(std::count_if(
      impl_->sessions.begin(), impl_->sessions.end(), [](const std::shared_ptr<TcpSession> &session) {
        return session != nullptr && !session->closing && session->fd >= 0;
      }));
}

} // namespace aster::net

#endif
