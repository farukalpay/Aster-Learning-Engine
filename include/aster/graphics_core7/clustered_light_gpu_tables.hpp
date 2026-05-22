// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>

namespace aster {
struct ClusteredLightFrameData;
}

namespace aster::graphics_core7 {

struct ClusteredLightGpuTableReport {
  std::size_t cluster_count = 0u;
  std::size_t visible_light_count = 0u;
  std::size_t light_index_count = 0u;
  bool overflowed = false;
  bool fallback_used = false;
  bool gpu_consumption_proven = false;
  std::uint64_t table_hash = 0u;
};

[[nodiscard]] ClusteredLightGpuTableReport
inspectClusteredLightGpuTables(const ClusteredLightFrameData &data, bool native_gpu_consumed);

} // namespace aster::graphics_core7
