// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/net/net_message.hpp"
#include "aster/net/node_router.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#ifndef ASTER_HAS_NETWORK_TRANSPORT
#define ASTER_HAS_NETWORK_TRANSPORT 0
#endif

namespace aster::net {

#if ASTER_HAS_NETWORK_TRANSPORT

class TcpNode {
public:
  explicit TcpNode(NodeProfile profile = {});
  ~TcpNode();

  TcpNode(const TcpNode &) = delete;
  TcpNode &operator=(const TcpNode &) = delete;

  NodeRouter &router();
  [[nodiscard]] const NodeRouter &router() const;
  [[nodiscard]] const NodeProfile &profile() const;

  std::uint16_t listen(std::uint16_t port);
  void connect(const std::string &host, std::uint16_t port);
  void runAsync();
  void stop();
  void send(NetMessage message);

  [[nodiscard]] bool running() const;
  [[nodiscard]] std::size_t peerCount() const;

private:
  friend class TcpSession;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif

} // namespace aster::net
