// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct DescriptorHeapDesc {
  std::uint32_t descriptor_capacity = 0u;
  bool shader_visible = true;
  std::string debug_label;
};

} // namespace aster::rhi
