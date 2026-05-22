// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/framegraph/transient_resource_allocator.hpp"

namespace aster::graphics_core7 {

using TransientHeapPlan = framegraph::TransientResourceAllocationPlan;
using TransientHeapStats = framegraph::TransientResourceAllocationStats;

class TransientHeapAllocator {
public:
  [[nodiscard]] TransientHeapPlan plan(const framegraph::CompiledFrameGraph &graph) const;
};

} // namespace aster::graphics_core7
