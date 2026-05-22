// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/shadow_atlas_native.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

NativeShadowAtlasReport inspectNativeShadowAtlas(
    const bool requested, const bool resource_advertised, const bool capture_available,
    const bool sampled_by_final_frame, const std::uint32_t atlas_size,
    const std::uint32_t cascade_count) {
  NativeShadowAtlasReport report{.requested = requested,
                                 .resource_advertised = resource_advertised,
                                 .capture_available = capture_available,
                                 .sampled_by_final_frame = sampled_by_final_frame,
                                 .atlas_size = atlas_size,
                                 .cascade_count = cascade_count};
  std::uint64_t hash = hashCombine64(0xA57E700700000401ull, requested ? 1u : 0u);
  hash = hashCombine64(hash, resource_advertised ? 1u : 0u);
  hash = hashCombine64(hash, capture_available ? 1u : 0u);
  hash = hashCombine64(hash, sampled_by_final_frame ? 1u : 0u);
  hash = hashCombine64(hash, atlas_size);
  report.evidence_hash = hashCombine64(hash, cascade_count);
  return report;
}

} // namespace aster::graphics_core7
