// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/net/net_message.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

void appendU16(std::vector<std::uint8_t> &out, const std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendU32(std::vector<std::uint8_t> &out, const std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendU64(std::vector<std::uint8_t> &out, const std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>((value >> static_cast<unsigned>(shift)) & 0xffu));
  }
}

std::uint16_t readU16(const std::span<const std::uint8_t> data, const std::size_t offset) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[offset]) << 8u) |
                                    static_cast<std::uint16_t>(data[offset + 1u]));
}

std::uint32_t readU32(const std::span<const std::uint8_t> data, const std::size_t offset) {
  return (static_cast<std::uint32_t>(data[offset]) << 24u) |
         (static_cast<std::uint32_t>(data[offset + 1u]) << 16u) |
         (static_cast<std::uint32_t>(data[offset + 2u]) << 8u) |
         static_cast<std::uint32_t>(data[offset + 3u]);
}

std::uint64_t readU64(const std::span<const std::uint8_t> data, const std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8u; ++i) {
    value = (value << 8u) | static_cast<std::uint64_t>(data[offset + i]);
  }
  return value;
}

} // namespace

namespace aster::net {

std::vector<std::uint8_t> encodeFrame(const NetMessage &message) {
  if (message.payload.size() > kMaxPayloadBytes) {
    throw std::invalid_argument("Network payload exceeds the configured frame payload limit.");
  }

  const auto body_bytes = static_cast<std::uint32_t>(kFrameMetadataBytes + message.payload.size());
  std::vector<std::uint8_t> out;
  out.reserve(kFramePrefixBytes + body_bytes);
  appendU32(out, body_bytes);
  appendU16(out, message.channel);
  appendU32(out, message.sequence);
  appendU64(out, message.source_node);
  appendU64(out, message.target_node);
  out.insert(out.end(), message.payload.begin(), message.payload.end());
  return out;
}

FrameDecodeResult inspectFramePrefix(const std::span<const std::uint8_t> prefix,
                                     std::uint32_t &body_bytes) {
  if (prefix.size() < kFramePrefixBytes) {
    return FrameDecodeResult::Incomplete;
  }

  body_bytes = readU32(prefix, 0);
  if (body_bytes < kFrameMetadataBytes) {
    return FrameDecodeResult::Malformed;
  }
  if (body_bytes > kMaxFrameBodyBytes) {
    return FrameDecodeResult::Oversized;
  }
  return FrameDecodeResult::Ready;
}

FrameDecodeResult decodeNextFrame(std::vector<std::uint8_t> &buffer, NetMessage &message) {
  std::uint32_t body_bytes = 0;
  const FrameDecodeResult prefix_result = inspectFramePrefix(buffer, body_bytes);
  if (prefix_result != FrameDecodeResult::Ready) {
    return prefix_result;
  }

  const std::size_t frame_bytes = kFramePrefixBytes + static_cast<std::size_t>(body_bytes);
  if (buffer.size() < frame_bytes) {
    return FrameDecodeResult::Incomplete;
  }

  const std::span<const std::uint8_t> frame(buffer.data() + kFramePrefixBytes, body_bytes);
  message.channel = readU16(frame, 0);
  message.sequence = readU32(frame, 2);
  message.source_node = readU64(frame, 6);
  message.target_node = readU64(frame, 14);
  message.payload.assign(frame.begin() + static_cast<std::ptrdiff_t>(kFrameMetadataBytes),
                         frame.end());

  buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(frame_bytes));
  return FrameDecodeResult::Ready;
}

std::vector<std::uint8_t> bytesFromText(const std::string_view text) {
  return {text.begin(), text.end()};
}

std::string textFromBytes(const std::span<const std::uint8_t> bytes) {
  return {bytes.begin(), bytes.end()};
}

} // namespace aster::net
