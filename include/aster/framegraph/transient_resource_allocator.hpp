// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aster::framegraph {

struct TransientResourceAllocationStats {
  std::size_t transient_resources = 0u;
  std::size_t lifetime_ranges = 0u;
  std::size_t physical_allocations = 0u;
  std::uint64_t transient_bytes = 0u;
  std::uint64_t aliased_bytes_saved = 0u;
};

struct TransientResourcePhysicalAllocation {
  std::size_t id = 0u;
  std::vector<ResourceHandle> resources;
  std::size_t first_pass = 0u;
  std::size_t last_pass = 0u;
  std::uint64_t byte_size = 0u;
};

struct TransientResourceAllocationPlan {
  std::vector<TransientResourcePhysicalAllocation> physical_allocations;
  TransientResourceAllocationStats stats{};
};

class TransientResourceAllocator {
public:
  [[nodiscard]] TransientResourceAllocationStats inspect(const CompiledFrameGraph &graph) const;
  [[nodiscard]] TransientResourceAllocationPlan allocate(const CompiledFrameGraph &graph) const;
};

} // namespace aster::framegraph
