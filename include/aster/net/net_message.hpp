// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aster::net {

using ChannelId = std::uint16_t;
using NodeId = std::uint64_t;

constexpr NodeId kUnassignedNodeId = 0;
constexpr std::size_t kFramePrefixBytes = 4;
constexpr std::size_t kFrameMetadataBytes = 22;
constexpr std::size_t kMaxPayloadBytes = 64u * 1024u;
constexpr std::size_t kMaxFrameBodyBytes = kFrameMetadataBytes + kMaxPayloadBytes;

struct NetMessage {
  ChannelId channel = 0;
  std::uint32_t sequence = 0;
  NodeId source_node = kUnassignedNodeId;
  NodeId target_node = kUnassignedNodeId;
  std::vector<std::uint8_t> payload;
};

enum class FrameDecodeResult {
  Incomplete,
  Ready,
  Oversized,
  Malformed,
};

std::vector<std::uint8_t> encodeFrame(const NetMessage &message);
FrameDecodeResult inspectFramePrefix(std::span<const std::uint8_t> prefix,
                                     std::uint32_t &body_bytes);
FrameDecodeResult decodeNextFrame(std::vector<std::uint8_t> &buffer, NetMessage &message);

std::vector<std::uint8_t> bytesFromText(std::string_view text);
std::string textFromBytes(std::span<const std::uint8_t> bytes);

} // namespace aster::net
