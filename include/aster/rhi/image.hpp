// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class ImageFormat : std::uint32_t {
  Unknown,
  Rgba8Unorm,
  Rgba8Srgb,
  Bgra8Unorm,
  Bgra8Srgb,
  Rgba16Float,
  Rg8Unorm,
  R8Unorm,
  Bc1RgbaUnorm,
  Bc1RgbaSrgb,
  Bc3RgbaUnorm,
  Bc3RgbaSrgb,
  Bc5RgUnorm,
  Bc7RgbaUnorm,
  Bc7RgbaSrgb,
  Astc4x4RgbaUnorm,
  Astc4x4RgbaSrgb,
  Depth32Float,
};

enum class ImageUsage : std::uint32_t {
  Sampled = 1u << 0u,
  ColorAttachment = 1u << 1u,
  DepthAttachment = 1u << 2u,
  TransferSource = 1u << 3u,
  TransferDestination = 1u << 4u,
  Storage = 1u << 5u,
};

[[nodiscard]] constexpr std::uint32_t imageUsageBit(const ImageUsage usage) {
  return static_cast<std::uint32_t>(usage);
}

struct ImageExtent {
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::uint32_t depth = 1u;
};

struct ImageDesc {
  ImageFormat format = ImageFormat::Unknown;
  ImageExtent extent{};
  std::uint32_t mip_levels = 1u;
  std::uint32_t array_layers = 1u;
  std::uint32_t usage = 0u;
  std::string debug_label;
};

using TextureHandle = ImageHandle;
using TextureDesc = ImageDesc;

} // namespace aster::rhi
