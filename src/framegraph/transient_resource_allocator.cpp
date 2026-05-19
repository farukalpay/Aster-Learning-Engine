// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/framegraph/transient_resource_allocator.hpp"

#include <algorithm>
#include <utility>

namespace aster::framegraph {
namespace {

std::uint64_t estimatedBytesFor(const ResourceDesc &desc) {
  if (desc.kind == ResourceKind::Buffer) {
    return std::max<std::uint64_t>(desc.byte_size, desc.stride);
  }
  std::uint64_t bytes_per_pixel = 4u;
  switch (desc.format) {
  case rhi::ImageFormat::Rgba16Float:
    bytes_per_pixel = 8u;
    break;
  case rhi::ImageFormat::Rg8Unorm:
    bytes_per_pixel = 2u;
    break;
  case rhi::ImageFormat::R8Unorm:
    bytes_per_pixel = 1u;
    break;
  default:
    bytes_per_pixel = 4u;
    break;
  }
  const std::uint64_t width = std::max(desc.extent.width, 1u);
  const std::uint64_t height = std::max(desc.extent.height, 1u);
  const std::uint64_t depth = std::max(desc.extent.depth, 1u);
  return width * height * depth * bytes_per_pixel;
}

} // namespace

TransientResourceAllocationStats
TransientResourceAllocator::inspect(const CompiledFrameGraph &graph) const {
  return allocate(graph).stats;
}

TransientResourceAllocationPlan
TransientResourceAllocator::allocate(const CompiledFrameGraph &graph) const {
  TransientResourceAllocationPlan plan;
  std::uint64_t unaliased_bytes = 0u;
  for (const CompiledResource &resource : graph.resources) {
    if (resource.desc.lifetime == ResourceLifetime::Transient) {
      ++plan.stats.transient_resources;
      if (resource.last_pass >= resource.first_pass) {
        ++plan.stats.lifetime_ranges;
      }
      unaliased_bytes += estimatedBytesFor(resource.desc);
    }
  }
  for (const ResourceAliasGroup &group : graph.alias_groups) {
    TransientResourcePhysicalAllocation allocation;
    allocation.id = group.id;
    allocation.resources = group.resources;
    allocation.first_pass = group.first_pass;
    allocation.last_pass = group.last_pass;
    for (const ResourceHandle handle : group.resources) {
      const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
                                      [handle](const CompiledResource &resource) {
                                        return resource.handle == handle;
                                      });
      if (found != graph.resources.end()) {
        allocation.byte_size = std::max(allocation.byte_size, estimatedBytesFor(found->desc));
      }
    }
    if (!allocation.resources.empty()) {
      plan.stats.transient_bytes += allocation.byte_size;
      plan.physical_allocations.push_back(std::move(allocation));
    }
  }
  plan.stats.physical_allocations = plan.physical_allocations.size();
  plan.stats.aliased_bytes_saved =
      unaliased_bytes > plan.stats.transient_bytes ? unaliased_bytes - plan.stats.transient_bytes
                                                   : 0u;
  return plan;
}

} // namespace aster::framegraph
