// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/deterministic_sim.hpp"
#include "aster/net/net_message.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace aster::net {

struct LockstepCommandPacket {
  NodeId player = kUnassignedNodeId;
  std::uint32_t start_tick = 0;
  std::uint32_t request_resend_from = 0;
  bool requests_resend = false;
  std::vector<SimCommand> commands;
};

class LockstepCommandChannel {
public:
  explicit LockstepCommandChannel(ChannelId channel = 17u);

  void pushLocal(SimCommand command);
  [[nodiscard]] NetMessage buildMessage(NodeId source, NodeId target, std::size_t max_commands);
  [[nodiscard]] bool receive(const NetMessage &message);
  [[nodiscard]] std::optional<SimCommand> commandForTick(std::uint32_t tick) const;
  [[nodiscard]] std::uint32_t checksum() const;
  [[nodiscard]] ChannelId channel() const;
  [[nodiscard]] std::uint32_t nextExpectedTick() const;
  void requestResend(std::uint32_t tick);

private:
  ChannelId channel_ = 17u;
  std::vector<SimCommand> local_;
  std::vector<SimCommand> remote_;
  std::uint32_t next_expected_tick_ = 0;
  std::uint32_t resend_from_ = 0;
  bool resend_requested_ = false;
};

[[nodiscard]] std::vector<std::uint8_t> encodeLockstepPacket(const LockstepCommandPacket &packet);
[[nodiscard]] std::optional<LockstepCommandPacket> decodeLockstepPacket(
    std::span<const std::uint8_t> bytes);

} // namespace aster::net
