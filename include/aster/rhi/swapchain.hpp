// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct SwapchainDesc {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  ImageFormat format = ImageFormat::Bgra8Unorm;
  std::uint32_t image_count = 2u;
  std::string debug_label;
};

} // namespace aster::rhi
