// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct ImageViewDesc {
  ImageHandle image{};
  ImageFormat format = ImageFormat::Unknown;
  std::uint32_t base_mip = 0u;
  std::uint32_t mip_count = 1u;
  std::string debug_label;
};

} // namespace aster::rhi
