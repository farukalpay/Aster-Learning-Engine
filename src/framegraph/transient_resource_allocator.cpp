// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/framegraph/transient_resource_allocator.hpp"

namespace aster::framegraph {

TransientResourceAllocationStats
TransientResourceAllocator::inspect(const CompiledFrameGraph &graph) const {
  TransientResourceAllocationStats stats;
  for (const CompiledResource &resource : graph.resources) {
    if (resource.desc.lifetime == ResourceLifetime::Transient) {
      ++stats.transient_resources;
      if (resource.last_pass >= resource.first_pass) {
        ++stats.lifetime_ranges;
      }
    }
  }
  return stats;
}

} // namespace aster::framegraph
