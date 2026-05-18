// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct DescriptorSetDesc {
  DescriptorHeapHandle heap{};
  PipelineLayoutHandle layout{};
  std::uint32_t set_index = 0u;
  std::uint32_t descriptor_count = 0u;
  std::string debug_label;
};

} // namespace aster::rhi
