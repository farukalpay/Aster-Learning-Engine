// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/clustered_light_gpu_tables.hpp"

#include "aster/math/hash.hpp"
#include "aster/render/render_device.hpp"

namespace aster::graphics_core7 {

ClusteredLightGpuTableReport
inspectClusteredLightGpuTables(const ClusteredLightFrameData &data,
                               const bool native_gpu_consumed) {
  ClusteredLightGpuTableReport report;
  report.cluster_count = static_cast<std::size_t>(data.cluster_count_x) * data.cluster_count_y *
                         data.cluster_count_z;
  report.visible_light_count = data.visible_lights.size();
  report.light_index_count = data.light_indices.size();
  report.overflowed = data.overflowed;
  report.fallback_used = data.fallback_used;
  report.gpu_consumption_proven = native_gpu_consumed;
  std::uint64_t hash = hashCombine64(0xA57E700700000301ull, data.visible_lights_hash);
  hash = hashCombine64(hash, data.assignments_hash);
  report.table_hash = hashCombine64(hash, native_gpu_consumed ? 1u : 0u);
  return report;
}

} // namespace aster::graphics_core7
