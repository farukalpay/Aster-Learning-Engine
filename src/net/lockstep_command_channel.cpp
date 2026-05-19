// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/lockstep_command_channel.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>

namespace {

void writeU16(std::vector<std::uint8_t> &out, const std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void writeU32(std::vector<std::uint8_t> &out, const std::uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu));
  }
}

void writeU64(std::vector<std::uint8_t> &out, const std::uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu));
  }
}

std::uint16_t readU16(std::span<const std::uint8_t> bytes, std::size_t &cursor) {
  const std::uint16_t value = static_cast<std::uint16_t>(bytes[cursor]) |
                              (static_cast<std::uint16_t>(bytes[cursor + 1u]) << 8u);
  cursor += 2u;
  return value;
}

std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t &cursor) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<std::uint32_t>(bytes[cursor++]) << (i * 8);
  }
  return value;
}

std::uint64_t readU64(std::span<const std::uint8_t> bytes, std::size_t &cursor) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(bytes[cursor++]) << (i * 8);
  }
  return value;
}

} // namespace

namespace aster::net {

LockstepCommandChannel::LockstepCommandChannel(const ChannelId channel) : channel_(channel) {}

void LockstepCommandChannel::pushLocal(SimCommand command) {
  local_.push_back(command);
}

NetMessage LockstepCommandChannel::buildMessage(const NodeId source, const NodeId target,
                                                const std::size_t max_commands) {
  LockstepCommandPacket packet;
  packet.player = source;
  packet.request_resend_from = resend_from_;
  packet.requests_resend = resend_requested_;
  const std::size_t count = std::min(max_commands, local_.size());
  if (count > 0u) {
    packet.start_tick = local_[local_.size() - count].tick;
    packet.commands.assign(local_.end() - static_cast<std::ptrdiff_t>(count), local_.end());
  }
  resend_requested_ = false;
  return {.channel = channel_,
          .sequence = packet.start_tick,
          .source_node = source,
          .target_node = target,
          .payload = encodeLockstepPacket(packet)};
}

bool LockstepCommandChannel::receive(const NetMessage &message) {
  if (message.channel != channel_) {
    return false;
  }
  const std::optional<LockstepCommandPacket> packet = decodeLockstepPacket(message.payload);
  if (!packet.has_value()) {
    return false;
  }
  for (const SimCommand &command : packet->commands) {
    if (command.tick < next_expected_tick_) {
      continue;
    }
    if (command.tick > next_expected_tick_) {
      requestResend(next_expected_tick_);
      return false;
    }
    remote_.push_back(command);
    ++next_expected_tick_;
  }
  return true;
}

std::optional<SimCommand> LockstepCommandChannel::commandForTick(const std::uint32_t tick) const {
  const auto found = std::find_if(remote_.begin(), remote_.end(),
                                  [tick](const SimCommand &command) {
                                    return command.tick == tick;
                                  });
  if (found == remote_.end()) {
    return std::nullopt;
  }
  return *found;
}

std::uint32_t LockstepCommandChannel::checksum() const {
  std::uint32_t seed = checksumCommands(local_);
  return checksumCommands(remote_, seed);
}

ChannelId LockstepCommandChannel::channel() const {
  return channel_;
}

std::uint32_t LockstepCommandChannel::nextExpectedTick() const {
  return next_expected_tick_;
}

void LockstepCommandChannel::requestResend(const std::uint32_t tick) {
  resend_from_ = tick;
  resend_requested_ = true;
}

std::vector<std::uint8_t> encodeLockstepPacket(const LockstepCommandPacket &packet) {
  std::vector<std::uint8_t> out;
  writeU64(out, packet.player);
  writeU32(out, packet.start_tick);
  writeU32(out, packet.request_resend_from);
  writeU16(out, static_cast<std::uint16_t>(packet.requests_resend ? 1u : 0u));
  writeU16(out, static_cast<std::uint16_t>(std::min<std::size_t>(packet.commands.size(), 255u)));
  for (const SimCommand &command : packet.commands) {
    writeU32(out, command.tick);
    writeU16(out, static_cast<std::uint16_t>(command.forward));
    writeU16(out, static_cast<std::uint16_t>(command.strafe));
    writeU16(out, static_cast<std::uint16_t>(command.turn));
    writeU16(out, static_cast<std::uint16_t>(command.look));
    writeU32(out, command.buttons);
    writeU32(out, command.sequence);
  }
  writeU32(out, checksumCommands(packet.commands));
  return out;
}

std::optional<LockstepCommandPacket> decodeLockstepPacket(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 24u) {
    return std::nullopt;
  }
  std::size_t cursor = 0;
  LockstepCommandPacket packet;
  packet.player = readU64(bytes, cursor);
  packet.start_tick = readU32(bytes, cursor);
  packet.request_resend_from = readU32(bytes, cursor);
  packet.requests_resend = readU16(bytes, cursor) != 0u;
  const std::uint16_t count = readU16(bytes, cursor);
  const std::size_t command_bytes = static_cast<std::size_t>(count) * 20u;
  if (bytes.size() != cursor + command_bytes + 4u) {
    return std::nullopt;
  }
  packet.commands.reserve(count);
  for (std::uint16_t i = 0; i < count; ++i) {
    SimCommand command;
    command.tick = readU32(bytes, cursor);
    command.forward = static_cast<std::int16_t>(readU16(bytes, cursor));
    command.strafe = static_cast<std::int16_t>(readU16(bytes, cursor));
    command.turn = static_cast<std::int16_t>(readU16(bytes, cursor));
    command.look = static_cast<std::int16_t>(readU16(bytes, cursor));
    command.buttons = readU32(bytes, cursor);
    command.sequence = readU32(bytes, cursor);
    packet.commands.push_back(command);
  }
  const std::uint32_t expected = readU32(bytes, cursor);
  if (checksumCommands(packet.commands) != expected) {
    return std::nullopt;
  }
  return packet;
}

} // namespace aster::net
