// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/graphics_core7.hpp"

#include "aster/math/hash.hpp"

#include <bit>

namespace aster::graphics_core7 {
namespace {

constexpr std::uint64_t kSeed = 0xA57E700700000001ull;

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kSeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::uint64_t mixText(std::uint64_t hash, const std::string_view text) {
  for (const char c : text) {
    hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return hash;
}

} // namespace

std::string_view signalName(const Signal signal) noexcept {
  switch (signal) {
  case Signal::SurfaceTruthBuffer:
    return "surface_truth_buffer";
  case Signal::LightTransportEvidence:
    return "light_transport_evidence";
  case Signal::ShadowContinuityField:
    return "shadow_continuity_field";
  case Signal::ReflectionProbeResidency:
    return "reflection_probe_residency";
  case Signal::MaterialFrequencyAudit:
    return "material_frequency_audit";
  case Signal::TemporalStabilityAudit:
    return "temporal_stability_audit";
  case Signal::BackendVisualDelta:
    return "backend_visual_delta";
  case Signal::PlayerReadableFrameVerdict:
    return "player_readable_frame_verdict";
  }
  return "surface_truth_buffer";
}

std::string_view signalStatusName(const SignalStatus status) noexcept {
  switch (status) {
  case SignalStatus::NotRequired:
    return "not_required";
  case SignalStatus::Proven:
    return "proven";
  case SignalStatus::Degraded:
    return "degraded";
  case SignalStatus::MissingProof:
    return "missing_proof";
  case SignalStatus::Unsupported:
    return "unsupported";
  }
  return "missing_proof";
}

std::string_view verdictStatusName(const VerdictStatus status) noexcept {
  switch (status) {
  case VerdictStatus::Accepted:
    return "accepted";
  case VerdictStatus::Degraded:
    return "degraded";
  case VerdictStatus::Rejected:
    return "rejected";
  }
  return "rejected";
}

std::uint64_t signalEvidenceHash(const SignalEvidence &evidence) noexcept {
  std::uint64_t hash = mixText(kSeed, "aster.graphics-core7.signal.v1");
  hash = mix(hash, static_cast<std::uint64_t>(evidence.signal));
  hash = mix(hash, static_cast<std::uint64_t>(evidence.status));
  hash = mix(hash, static_cast<std::uint64_t>(evidence.pass));
  hash = mix(hash, static_cast<std::uint64_t>(evidence.resource));
  hash = mix(hash, evidence.required ? 1ull : 0ull);
  hash = mix(hash, evidence.score);
  hash = mix(hash, evidence.threshold);
  hash = mixText(hash, evidence.label);
  hash = mixText(hash, evidence.evidence);
  return mixText(hash, evidence.message);
}

std::uint64_t verdictHash(const PlayerReadableFrameVerdict &verdict,
                          const std::vector<SignalEvidence> &signals) noexcept {
  std::uint64_t hash = mixText(kSeed, "aster.graphics-core7.verdict.v1");
  hash = mix(hash, static_cast<std::uint64_t>(verdict.status));
  hash = mix(hash, verdict.accepted ? 1ull : 0ull);
  hash = mix(hash, verdict.strict ? 1ull : 0ull);
  hash = mix(hash, verdict.score);
  hash = mix(hash, verdict.minimum_score);
  hash = mix(hash, static_cast<std::uint64_t>(verdict.required_signal_mask));
  hash = mix(hash, static_cast<std::uint64_t>(verdict.proven_signal_mask));
  hash = mix(hash, static_cast<std::uint64_t>(verdict.degraded_signal_mask));
  hash = mix(hash, static_cast<std::uint64_t>(verdict.missing_signal_mask));
  hash = mix(hash, static_cast<std::uint64_t>(verdict.unsupported_signal_mask));
  hash = mix(hash, static_cast<std::uint64_t>(verdict.rejected_signal_count));
  for (const SignalEvidence &signal : signals) {
    hash = mix(hash, signal.evidence_hash);
  }
  return mixText(hash, verdict.diagnostic);
}

} // namespace aster::graphics_core7
