// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/net/net_message.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aster::net {

struct NodeProfile {
  NodeId id = kUnassignedNodeId;
  std::string name;
  std::vector<std::string> capabilities;
};

struct RouteStats {
  std::size_t delivered = 0;
  std::size_t dropped = 0;
  std::size_t queued = 0;
};

class NodeDirectory {
public:
  void upsert(NodeProfile profile);
  bool remove(NodeId id);
  [[nodiscard]] std::optional<NodeProfile> find(NodeId id) const;
  [[nodiscard]] std::vector<NodeProfile> nodes() const;

private:
  std::vector<NodeProfile> nodes_;
};

class NodeRouter {
public:
  using SubscriptionToken = std::uint64_t;
  using Handler = std::function<void(const NetMessage &)>;

  SubscriptionToken subscribe(ChannelId channel, Handler handler);
  bool unsubscribe(SubscriptionToken token);

  bool route(const NetMessage &message);
  void enqueue(NetMessage message);
  std::size_t drain(std::size_t max_messages);
  std::size_t drainAll();

  [[nodiscard]] bool hasSubscribers(ChannelId channel) const;
  [[nodiscard]] const RouteStats &stats() const;

private:
  struct Subscription {
    SubscriptionToken token = 0;
    ChannelId channel = 0;
    Handler handler;
  };

  std::vector<Subscription> subscriptions_;
  std::deque<NetMessage> queue_;
  RouteStats stats_;
  SubscriptionToken next_token_ = 1;
};

} // namespace aster::net
