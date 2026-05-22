// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>

namespace aster::graphics_core7 {

struct TemporalStabilityReport {
  std::size_t timestamp_sample_count = 0u;
  std::size_t available_timestamp_count = 0u;
  bool motion_sensitive_materials = false;
  bool stable = true;
  float score = 1.0f;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] TemporalStabilityReport inspectTemporalStability(
    std::size_t timestamp_sample_count, std::size_t available_timestamp_count,
    bool motion_sensitive_materials, bool strict_native_timestamps);

} // namespace aster::graphics_core7
