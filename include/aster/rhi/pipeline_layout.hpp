// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>
#include <vector>

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
  std::vector<DescriptorRangeDesc> descriptor_ranges;
  std::uint64_t layout_hash = 0u;
  std::string debug_label;
};

namespace detail {

[[nodiscard]] constexpr std::uint64_t descriptorFnv1aAppend(std::uint64_t hash,
                                                            const std::uint64_t value) {
  constexpr std::uint64_t kPrime = 1099511628211ull;
  hash ^= value;
  hash *= kPrime;
  return hash;
}

} // namespace detail

[[nodiscard]] inline std::uint64_t descriptorLayoutHash(const PipelineLayoutDesc &desc) {
  if (desc.layout_hash != 0u) {
    return desc.layout_hash;
  }
  std::uint64_t hash = 1469598103934665603ull;
  const std::uint32_t count = desc.descriptor_ranges.empty()
                                  ? desc.descriptor_range_count
                                  : static_cast<std::uint32_t>(desc.descriptor_ranges.size());
  hash = detail::descriptorFnv1aAppend(hash, count);
  for (const DescriptorRangeDesc &range : desc.descriptor_ranges) {
    hash = detail::descriptorFnv1aAppend(hash, static_cast<std::uint64_t>(range.kind));
    hash = detail::descriptorFnv1aAppend(hash, range.binding);
    hash = detail::descriptorFnv1aAppend(hash, range.count);
    hash = detail::descriptorFnv1aAppend(hash, range.stage_mask);
  }
  return hash;
}

} // namespace aster::rhi
