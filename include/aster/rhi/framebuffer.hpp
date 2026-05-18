// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/render_target.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct FramebufferDesc {
  RenderTargetHandle render_target{};
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::string debug_label;
};

} // namespace aster::rhi
