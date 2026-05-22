// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>

namespace aster::graphics_core7 {

struct BackendVisualDeltaReport {
  std::size_t capture_count = 0u;
  std::size_t available_capture_count = 0u;
  std::size_t missing_proof_count = 0u;
  float score = 1.0f;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] BackendVisualDeltaReport inspectBackendVisualDelta(
    std::size_t capture_count, std::size_t available_capture_count, std::size_t missing_proof_count);

} // namespace aster::graphics_core7
