// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/transient_heap_allocator.hpp"

namespace aster::graphics_core7 {

TransientHeapPlan TransientHeapAllocator::plan(const framegraph::CompiledFrameGraph &graph) const {
  return framegraph::TransientResourceAllocator{}.allocate(graph);
}

} // namespace aster::graphics_core7
