// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"

#include <cstddef>

namespace aster::framegraph {

struct TransientResourceAllocationStats {
  std::size_t transient_resources = 0u;
  std::size_t lifetime_ranges = 0u;
};

class TransientResourceAllocator {
public:
  [[nodiscard]] TransientResourceAllocationStats inspect(const CompiledFrameGraph &graph) const;
};

} // namespace aster::framegraph
