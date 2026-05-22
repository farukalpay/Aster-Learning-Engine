// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/gpu_visibility_compaction.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

VisibilityCompactionReport
compactGpuVisibility(const std::vector<VisibilityCompactionInput> &inputs) {
  VisibilityCompactionReport report;
  report.input_count = inputs.size();
  std::uint64_t hash = 0xA57E700700000201ull;
  for (const VisibilityCompactionInput &input : inputs) {
    if (!input.visible) {
      continue;
    }
    ++report.visible_count;
    report.visible_indices.push_back(input.object_index);
    hash = hashCombine64(hash, static_cast<std::uint64_t>(input.object_index));
    hash = hashCombine64(hash, input.draw_key);
  }
  report.draw_key_hash = hash;
  return report;
}

} // namespace aster::graphics_core7
