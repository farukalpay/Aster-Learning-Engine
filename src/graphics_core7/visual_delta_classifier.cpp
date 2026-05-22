// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/visual_delta_classifier.hpp"

#include "aster/math/hash.hpp"

#include <bit>

namespace aster::graphics_core7 {
namespace {

[[nodiscard]] std::uint64_t mixDouble(std::uint64_t hash, const double value) {
  return hashCombine64(hash, std::bit_cast<std::uint64_t>(value));
}

} // namespace

std::string_view visualDeltaRootCauseName(const VisualDeltaRootCause cause) noexcept {
  switch (cause) {
  case VisualDeltaRootCause::None:
    return "none";
  case VisualDeltaRootCause::BackendProofDrift:
    return "backend_proof_drift";
  case VisualDeltaRootCause::MaterialOrToneMapDrift:
    return "material_or_tonemap_drift";
  case VisualDeltaRootCause::GeometrySilhouetteDrift:
    return "geometry_silhouette_drift";
  case VisualDeltaRootCause::TemporalInstability:
    return "temporal_instability";
  case VisualDeltaRootCause::GoldenBaselineStale:
    return "golden_baseline_stale";
  case VisualDeltaRootCause::UnknownVisualDrift:
    return "unknown_visual_drift";
  }
  return "unknown_visual_drift";
}

VisualDeltaClassification classifyVisualDelta(const VisualDeltaMetrics &metrics) {
  VisualDeltaClassification out;
  out.mean_abs_error = metrics.mean_abs_error;
  out.differing_pixel_ratio = metrics.differing_pixel_ratio;
  out.max_abs_error = metrics.max_abs_error;
  out.failed_budget = metrics.mean_abs_error > 2.0 || metrics.max_abs_error > 12u ||
                      metrics.differing_pixel_ratio > 0.04;
  if (!out.failed_budget) {
    out.root_cause = VisualDeltaRootCause::None;
    out.label = "visual_delta_within_budget";
    out.message = "Image delta remains inside the player-readable visual budget.";
  } else if (!metrics.backend_certified || !metrics.graphics_core7_accepted ||
             !metrics.same_backend_reference) {
    out.root_cause = VisualDeltaRootCause::BackendProofDrift;
    out.label = "backend_proof_drift";
    out.message =
        "The visual delta is coupled to backend or GraphicsCore7 proof failure.";
  } else if (!metrics.temporal_stability_proven) {
    out.root_cause = VisualDeltaRootCause::TemporalInstability;
    out.label = "temporal_instability";
    out.message = "The visual delta is coupled to missing temporal-stability proof.";
  } else if (!metrics.material_frequency_proven ||
             (metrics.max_abs_error >= 128u && metrics.mean_abs_error >= 6.0)) {
    out.root_cause = VisualDeltaRootCause::MaterialOrToneMapDrift;
    out.label = "material_or_tonemap_drift";
    out.message =
        "Same-backend golden mismatch points at material response, color space, or tonemap drift.";
  } else if (metrics.differing_pixel_ratio > 0.45) {
    out.root_cause = VisualDeltaRootCause::GeometrySilhouetteDrift;
    out.label = "geometry_silhouette_drift";
    out.message = "The diff covers enough pixels to suspect silhouette, camera, or geometry drift.";
  } else if (metrics.same_backend_reference && metrics.backend_certified &&
             metrics.graphics_core7_accepted) {
    out.root_cause = VisualDeltaRootCause::GoldenBaselineStale;
    out.label = "golden_baseline_stale_or_subtle_shader_drift";
    out.message =
        "Same-backend certified output differs from the checked-in golden; refresh needs review.";
  } else {
    out.root_cause = VisualDeltaRootCause::UnknownVisualDrift;
    out.label = "unknown_visual_drift";
    out.message = "The visual delta exceeds budget without a more specific proof root cause.";
  }

  std::uint64_t hash = hashCombine64(0xA57E700700000901ull,
                                     static_cast<std::uint64_t>(out.root_cause));
  hash = mixDouble(hash, out.mean_abs_error);
  hash = mixDouble(hash, out.differing_pixel_ratio);
  hash = hashCombine64(hash, out.max_abs_error);
  hash = hashCombine64(hash, out.failed_budget ? 1u : 0u);
  out.evidence_hash = hash;
  return out;
}

} // namespace aster::graphics_core7
