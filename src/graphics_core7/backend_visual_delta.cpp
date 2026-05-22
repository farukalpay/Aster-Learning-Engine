// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/backend_visual_delta.hpp"

#include "aster/math/hash.hpp"

namespace aster::graphics_core7 {

BackendVisualDeltaReport inspectBackendVisualDelta(const std::size_t capture_count,
                                                   const std::size_t available_capture_count,
                                                   const std::size_t missing_proof_count) {
  BackendVisualDeltaReport report{.capture_count = capture_count,
                                  .available_capture_count = available_capture_count,
                                  .missing_proof_count = missing_proof_count};
  const float capture_score = available_capture_count > 0u ? 1.0f : 0.0f;
  const float proof_penalty = missing_proof_count == 0u ? 1.0f : 0.35f;
  report.score = capture_score * proof_penalty;
  std::uint64_t hash = hashCombine64(0xA57E700700000801ull,
                                     static_cast<std::uint64_t>(capture_count));
  hash = hashCombine64(hash, static_cast<std::uint64_t>(available_capture_count));
  report.evidence_hash = hashCombine64(hash, static_cast<std::uint64_t>(missing_proof_count));
  return report;
}

} // namespace aster::graphics_core7
