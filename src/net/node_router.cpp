// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/node_router.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aster::net {

void NodeDirectory::upsert(NodeProfile profile) {
  const auto existing =
      std::find_if(nodes_.begin(), nodes_.end(),
                   [id = profile.id](const NodeProfile &node) { return node.id == id; });
  if (existing == nodes_.end()) {
    nodes_.push_back(std::move(profile));
  } else {
    *existing = std::move(profile);
  }
}

bool NodeDirectory::remove(const NodeId id) {
  const auto old_size = nodes_.size();
  nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                              [id](const NodeProfile &node) { return node.id == id; }),
               nodes_.end());
  return nodes_.size() != old_size;
}

std::optional<NodeProfile> NodeDirectory::find(const NodeId id) const {
  const auto existing = std::find_if(nodes_.begin(), nodes_.end(),
                                     [id](const NodeProfile &node) { return node.id == id; });
  if (existing == nodes_.end()) {
    return std::nullopt;
  }
  return *existing;
}

std::vector<NodeProfile> NodeDirectory::nodes() const {
  return nodes_;
}

NodeRouter::SubscriptionToken NodeRouter::subscribe(const ChannelId channel, Handler handler) {
  if (!handler) {
    throw std::invalid_argument("Network route subscription requires a valid handler.");
  }
  const SubscriptionToken token = next_token_++;
  subscriptions_.push_back({token, channel, std::move(handler)});
  return token;
}

bool NodeRouter::unsubscribe(const SubscriptionToken token) {
  const auto old_size = subscriptions_.size();
  subscriptions_.erase(
      std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                     [token](const Subscription &entry) { return entry.token == token; }),
      subscriptions_.end());
  return subscriptions_.size() != old_size;
}

bool NodeRouter::route(const NetMessage &message) {
  bool delivered = false;
  for (const Subscription &entry : subscriptions_) {
    if (entry.channel == message.channel) {
      entry.handler(message);
      delivered = true;
      ++stats_.delivered;
    }
  }

  if (!delivered) {
    ++stats_.dropped;
  }
  return delivered;
}

void NodeRouter::enqueue(NetMessage message) {
  queue_.push_back(std::move(message));
  ++stats_.queued;
}

std::size_t NodeRouter::drain(const std::size_t max_messages) {
  std::size_t routed = 0;
  while (!queue_.empty() && routed < max_messages) {
    NetMessage message = std::move(queue_.front());
    queue_.pop_front();
    route(message);
    ++routed;
  }
  return routed;
}

std::size_t NodeRouter::drainAll() {
  return drain(std::numeric_limits<std::size_t>::max());
}

bool NodeRouter::hasSubscribers(const ChannelId channel) const {
  return std::any_of(subscriptions_.begin(), subscriptions_.end(),
                     [channel](const Subscription &entry) { return entry.channel == channel; });
}

const RouteStats &NodeRouter::stats() const {
  return stats_;
}

} // namespace aster::net
