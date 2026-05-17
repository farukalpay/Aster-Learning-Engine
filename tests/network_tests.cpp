// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

namespace {

void testNetworkFrameCodec() {
  aster::net::NetMessage message;
  message.channel = 42;
  message.sequence = 9;
  message.source_node = 11;
  message.target_node = 12;
  message.payload = aster::net::bytesFromText("hello");

  std::vector<std::uint8_t> frame = aster::net::encodeFrame(message);
  aster::net::NetMessage decoded;
  assert(aster::net::decodeNextFrame(frame, decoded) == aster::net::FrameDecodeResult::Ready);
  assert(frame.empty());
  assert(decoded.channel == message.channel);
  assert(decoded.sequence == message.sequence);
  assert(decoded.source_node == message.source_node);
  assert(decoded.target_node == message.target_node);
  assert(aster::net::textFromBytes(decoded.payload) == "hello");

  std::vector<std::uint8_t> partial = aster::net::encodeFrame(message);
  partial.pop_back();
  assert(aster::net::decodeNextFrame(partial, decoded) ==
         aster::net::FrameDecodeResult::Incomplete);
}

void testNodeRouter() {
  aster::net::NodeRouter router;
  int delivered = 0;
  const auto token = router.subscribe(7, [&](const aster::net::NetMessage &message) {
    delivered += message.sequence == 5 ? 1 : 0;
  });

  aster::net::NetMessage message;
  message.channel = 7;
  message.sequence = 5;
  router.enqueue(message);
  assert(router.drainAll() == 1u);
  assert(delivered == 1);
  assert(router.stats().delivered == 1u);
  assert(router.unsubscribe(token));
  assert(!router.route(message));
  assert(router.stats().dropped == 1u);
}

} // namespace

int main() {
  testNetworkFrameCodec();
  testNodeRouter();
  std::cout << "network_tests passed.\n";
  return 0;
}
