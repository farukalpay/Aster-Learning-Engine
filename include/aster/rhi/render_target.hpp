// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/image_view.hpp"

#include <string>

namespace aster::rhi {

struct RenderTargetDesc {
  ImageViewHandle color{};
  ImageViewHandle depth{};
  std::string debug_label;
};

} // namespace aster::rhi
