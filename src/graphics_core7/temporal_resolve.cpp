// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/temporal_resolve.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

TemporalStabilityReport inspectTemporalStability(const std::size_t timestamp_sample_count,
                                                  const std::size_t available_timestamp_count,
                                                  const bool motion_sensitive_materials,
                                                  const bool strict_native_timestamps) {
  TemporalStabilityReport report{.timestamp_sample_count = timestamp_sample_count,
                                 .available_timestamp_count = available_timestamp_count,
                                 .motion_sensitive_materials = motion_sensitive_materials};
  const bool timestamps_ok = !strict_native_timestamps || available_timestamp_count > 0u;
  report.stable = timestamps_ok;
  report.score = timestamps_ok ? 1.0f : (motion_sensitive_materials ? 0.25f : 0.45f);
  std::uint64_t hash =
      hashCombine64(0xA57E700700000701ull, static_cast<std::uint64_t>(timestamp_sample_count));
  hash = hashCombine64(hash, static_cast<std::uint64_t>(available_timestamp_count));
  hash = hashCombine64(hash, motion_sensitive_materials ? 1u : 0u);
  report.evidence_hash = hashCombine64(hash, strict_native_timestamps ? 1u : 0u);
  return report;
}

} // namespace aster::graphics_core7
