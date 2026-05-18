// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class ImageViewDimension : std::uint32_t {
  Texture2D,
  Texture2DArray,
  TextureCube,
  TextureCubeArray,
};

struct ImageViewDesc {
  ImageHandle image{};
  ImageFormat format = ImageFormat::Unknown;
  ImageViewDimension dimension = ImageViewDimension::Texture2D;
  std::uint32_t base_mip = 0u;
  std::uint32_t mip_count = 1u;
  std::uint32_t base_layer = 0u;
  std::uint32_t layer_count = 1u;
  std::string debug_label;
};

} // namespace aster::rhi
