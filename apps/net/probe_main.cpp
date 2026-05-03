// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/tcp_node.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

constexpr aster::net::ChannelId kPingChannel = 1;
constexpr aster::net::ChannelId kPongChannel = 2;

template <typename Predicate>
bool waitUntil(Predicate predicate, const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate();
}

} // namespace

int main() {
#if !ASTER_HAS_NETWORK_TRANSPORT
  std::cerr << "Aster Learning Engine network transport was not enabled for this build.\n";
  return 1;
#else
  try {
    aster::net::TcpNode server({1, "loopback-server", {"authoritative"}});
    aster::net::TcpNode client({2, "loopback-client", {"playtest"}});

    std::atomic_bool received_ping = false;
    std::atomic_bool received_pong = false;

    server.router().subscribe(kPingChannel, [&](const aster::net::NetMessage &message) {
      received_ping = aster::net::textFromBytes(message.payload) == "ping";

      aster::net::NetMessage response;
      response.channel = kPongChannel;
      response.sequence = message.sequence;
      response.target_node = message.source_node;
      response.payload = aster::net::bytesFromText("pong");
      server.send(response);
    });

    client.router().subscribe(kPongChannel, [&](const aster::net::NetMessage &message) {
      received_pong = message.sequence == 7 && aster::net::textFromBytes(message.payload) == "pong";
    });

    const std::uint16_t port = server.listen(0);
    server.runAsync();

    client.connect("127.0.0.1", port);
    client.runAsync();

    const bool connected = waitUntil(
        [&] { return server.peerCount() > 0 && client.peerCount() > 0; }, std::chrono::seconds(2));
    if (!connected) {
      std::cerr << "Loopback peers did not connect in time.\n";
      return 1;
    }

    aster::net::NetMessage ping;
    ping.channel = kPingChannel;
    ping.sequence = 7;
    ping.target_node = server.profile().id;
    ping.payload = aster::net::bytesFromText("ping");
    client.send(ping);

    const bool exchanged = waitUntil([&] { return received_ping.load() && received_pong.load(); },
                                     std::chrono::seconds(2));

    client.stop();
    server.stop();

    if (!exchanged) {
      std::cerr << "Loopback ping/pong did not complete.\n";
      return 1;
    }

    std::cout << "Aster Learning Engine network probe passed on port " << port << ".\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Aster Learning Engine network probe failed: " << error.what() << '\n';
    return 1;
  }
#endif
}
