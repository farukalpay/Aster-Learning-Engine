// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/reflection_probe_atlas.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

ReflectionProbeAtlasReport inspectReflectionProbeAtlas(
    const bool requested, const bool resource_advertised, const bool capture_available,
    const bool sampled_by_final_frame, const std::size_t probe_count,
    const std::uint32_t face_size) {
  ReflectionProbeAtlasReport report{.requested = requested,
                                    .resource_advertised = resource_advertised,
                                    .capture_available = capture_available,
                                    .sampled_by_final_frame = sampled_by_final_frame,
                                    .probe_count = probe_count,
                                    .face_size = face_size};
  std::uint64_t hash = hashCombine64(0xA57E700700000601ull, requested ? 1u : 0u);
  hash = hashCombine64(hash, resource_advertised ? 1u : 0u);
  hash = hashCombine64(hash, capture_available ? 1u : 0u);
  hash = hashCombine64(hash, sampled_by_final_frame ? 1u : 0u);
  hash = hashCombine64(hash, static_cast<std::uint64_t>(probe_count));
  report.evidence_hash = hashCombine64(hash, face_size);
  return report;
}

} // namespace aster::graphics_core7
