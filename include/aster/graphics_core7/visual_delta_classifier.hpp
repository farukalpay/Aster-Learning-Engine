// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aster::graphics_core7 {

enum class VisualDeltaRootCause : std::uint32_t {
  None,
  BackendProofDrift,
  MaterialOrToneMapDrift,
  GeometrySilhouetteDrift,
  TemporalInstability,
  GoldenBaselineStale,
  UnknownVisualDrift,
};

struct VisualDeltaMetrics {
  double mean_abs_error = 0.0;
  double differing_pixel_ratio = 0.0;
  std::uint32_t max_abs_error = 0u;
  bool same_backend_reference = true;
  bool backend_certified = true;
  bool graphics_core7_accepted = true;
  bool material_frequency_proven = true;
  bool temporal_stability_proven = true;
};

struct VisualDeltaClassification {
  VisualDeltaRootCause root_cause = VisualDeltaRootCause::None;
  bool failed_budget = false;
  double mean_abs_error = 0.0;
  double differing_pixel_ratio = 0.0;
  std::uint32_t max_abs_error = 0u;
  std::string label;
  std::string message;
  std::uint64_t evidence_hash = 0u;
};

[[nodiscard]] std::string_view visualDeltaRootCauseName(VisualDeltaRootCause cause) noexcept;
[[nodiscard]] VisualDeltaClassification classifyVisualDelta(const VisualDeltaMetrics &metrics);

} // namespace aster::graphics_core7
