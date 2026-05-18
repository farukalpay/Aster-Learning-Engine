// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class DescriptorRangeKind : std::uint32_t {
  UniformBuffer,
  StorageBuffer,
  SampledImage,
  StorageImage,
  Sampler,
};

struct DescriptorRangeDesc {
  DescriptorRangeKind kind = DescriptorRangeKind::UniformBuffer;
  std::uint32_t binding = 0u;
  std::uint32_t count = 1u;
  std::uint32_t stage_mask = 0u;
};

struct PipelineLayoutDesc {
  std::uint32_t descriptor_range_count = 0u;
  std::string debug_label;
};

} // namespace aster::rhi
