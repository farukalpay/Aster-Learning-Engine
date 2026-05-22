// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/belief_extraction.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace aster {
namespace {

constexpr std::uint64_t kBeliefSeed = 0xA57EBE11EF000001ull;

[[nodiscard]] float clamp01(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kBeliefSeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const bool value) {
  return mix(hash, static_cast<std::uint64_t>(value ? 1u : 0u));
}

[[nodiscard]] std::uint64_t mixString(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return hash;
}

[[nodiscard]] float scoreWithDefault(const float value) {
  return std::isfinite(value) ? clamp01(value) : 0.0f;
}

[[nodiscard]] float materialFamilyUniqueness(const BeliefExtractionDesc &desc) {
  if (desc.visible_object_count == 0u || desc.material_family_count == 0u ||
      desc.visible_object_count < 4u) {
    return 1.0f;
  }
  return clamp01(static_cast<float>(desc.material_family_count) /
                 static_cast<float>(desc.visible_object_count));
}

[[nodiscard]] float contextualGroundingScore(const BeliefExtractionDesc &desc) {
  float score = 1.0f;
  if ((desc.perception_ledger.required_channel_mask &
       worldPerceptionLedgerChannelBit("contact_history")) != 0u &&
      desc.perception_ledger.contact_history_hash == 0u) {
    score = std::min(score, 0.0f);
  }
  if (desc.perception_ledger.required_channel_mask != 0u) {
    score = std::min(score, scoreWithDefault(desc.perception_ledger.score));
  }
  if (desc.has_scene_coherence) {
    score = std::min(score, clamp01(1.0f - desc.scene_coherence.energy));
  }
  return score;
}

[[nodiscard]] float contactShadowScore(const BeliefExtractionDesc &desc) {
  if (!desc.contact_shadow_required) {
    return 1.0f;
  }
  if (!desc.contact_shadow_enabled) {
    return 0.0f;
  }
  return scoreWithDefault(desc.contact_shadow_credibility);
}

[[nodiscard]] float volumetricCouplingScore(const BeliefExtractionDesc &desc) {
  if (!desc.volumetric_required &&
      (desc.perception_ledger.atmosphere_cell_hash == 0u ||
       desc.perception_ledger.lighting_exposure_hash == 0u)) {
    return 1.0f;
  }
  if (!desc.volumetric_scene_coupled) {
    return 0.0f;
  }
  return scoreWithDefault(desc.volumetric_scene_coupling);
}

[[nodiscard]] float materialResponseScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.material_response_stability);
  if (desc.perceptual_state.material_memory > 0.0f) {
    score = std::max(score, scoreWithDefault(desc.perceptual_state.material_memory));
  }
  if (desc.perception_ledger.material_memory_hash != 0u) {
    score = std::max(score, 0.72f);
  }
  return score;
}

[[nodiscard]] float environmentalEntropyScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.environmental_entropy);
  if (desc.perception_ledger.required_channel_mask != 0u) {
    const bool has_history = desc.perception_ledger.material_memory_hash != 0u &&
                             desc.perception_ledger.wear_continuity_hash != 0u &&
                             desc.perception_ledger.audio_visual_cue_budget_hash != 0u;
    score = std::min(score, has_history ? 1.0f : 0.35f);
  }
  if (desc.perceptual_state.ecology_signal > 0.0f) {
    score = std::max(score, scoreWithDefault(desc.perceptual_state.ecology_signal));
  }
  return score;
}

[[nodiscard]] std::uint64_t findingEvidenceHash(const BeliefExtractionDesc &desc,
                                                const BeliefFindingKind kind,
                                                const float score,
                                                const float threshold) {
  std::uint64_t hash = mixString(kBeliefSeed, beliefFindingKindName(kind));
  hash = mixString(hash, desc.subject);
  hash = mix(hash, desc.world_transition_hash);
  hash = mix(hash, desc.extraction_hash);
  hash = mix(hash, desc.source_hash);
  hash = mix(hash, desc.perception_ledger.ledger_hash);
  hash = mix(hash, desc.perceptual_state.perceptual_state_hash);
  hash = mix(hash, score);
  return mix(hash, threshold);
}

void addFinding(std::vector<BeliefExtractionFinding> &findings,
                const BeliefExtractionDesc &desc,
                const BeliefFindingKind kind,
                const float score,
                const float threshold,
                std::string source,
                std::string message) {
  if (score + 0.0001f >= threshold) {
    return;
  }
  findings.push_back({.kind = kind,
                      .severity = score < threshold * 0.50f ? BeliefFindingSeverity::Error
                                                            : BeliefFindingSeverity::Warning,
                      .subject = desc.subject,
                      .score = score,
                      .threshold = threshold,
                      .evidence_hash = findingEvidenceHash(desc, kind, score, threshold),
                      .source = std::move(source),
                      .message = std::move(message)});
}

[[nodiscard]] std::uint64_t hashReport(const BeliefExtractionDesc &desc,
                                       const BeliefExtractionReport &report) {
  std::uint64_t hash = mixString(kBeliefSeed, "aster.belief-extraction.report.v1");
  hash = mixString(hash, desc.subject);
  hash = mix(hash, desc.world_transition_hash);
  hash = mix(hash, desc.extraction_hash);
  hash = mix(hash, desc.source_hash);
  hash = mix(hash, desc.perception_ledger.ledger_hash);
  hash = mix(hash, desc.perceptual_state.perceptual_state_hash);
  hash = mix(hash, report.score);
  hash = mix(hash, report.minimum_score);
  hash = mix(hash, report.accepted);
  for (const BeliefExtractionFinding &finding : report.findings) {
    hash = mix(hash, finding.evidence_hash);
  }
  return hash;
}

} // namespace

std::string_view beliefFindingKindName(const BeliefFindingKind kind) noexcept {
  switch (kind) {
  case BeliefFindingKind::MaterialFamilyCollapse:
    return "material_family_collapse";
  case BeliefFindingKind::ContextualGroundingFailure:
    return "contextual_grounding_failure";
  case BeliefFindingKind::ContactShadowCredibilityFailure:
    return "contact_shadow_credibility_failure";
  case BeliefFindingKind::VolumetricSceneCouplingFailure:
    return "volumetric_scene_coupling_failure";
  case BeliefFindingKind::MaterialResponseInstability:
    return "material_response_instability";
  case BeliefFindingKind::LodTransitionVisibility:
    return "lod_transition_visibility";
  case BeliefFindingKind::AssetScaleIncoherence:
    return "asset_scale_incoherence";
  case BeliefFindingKind::EnvironmentalEntropyDeficit:
    return "environmental_entropy_deficit";
  }
  return "material_family_collapse";
}

std::string_view beliefFindingSeverityName(const BeliefFindingSeverity severity) noexcept {
  switch (severity) {
  case BeliefFindingSeverity::Info:
    return "info";
  case BeliefFindingSeverity::Warning:
    return "warning";
  case BeliefFindingSeverity::Error:
    return "error";
  }
  return "warning";
}

BeliefExtractionReport extractBeliefContract(const BeliefExtractionDesc &desc) {
  BeliefExtractionReport report;
  report.minimum_score = clamp01(desc.minimum_score);

  constexpr float kMaterialFamilyThreshold = 0.34f;
  constexpr float kGroundingThreshold = 0.62f;
  constexpr float kContactThreshold = 0.64f;
  constexpr float kVolumetricThreshold = 0.58f;
  constexpr float kMaterialResponseThreshold = 0.62f;
  constexpr float kLodThreshold = 0.70f;
  constexpr float kScaleThreshold = 0.70f;
  constexpr float kEntropyThreshold = 0.58f;

  const float material_family = materialFamilyUniqueness(desc);
  const float grounding = contextualGroundingScore(desc);
  const float contact = contactShadowScore(desc);
  const float volumetric = volumetricCouplingScore(desc);
  const float material_response = materialResponseScore(desc);
  const float lod = scoreWithDefault(desc.lod_transition_invisibility);
  const float scale = scoreWithDefault(desc.asset_scale_coherence);
  const float entropy = environmentalEntropyScore(desc);

  addFinding(report.findings, desc, BeliefFindingKind::MaterialFamilyCollapse,
             material_family, kMaterialFamilyThreshold, "material-family",
             "visible objects rely on too few material families for player-believable variety");
  addFinding(report.findings, desc, BeliefFindingKind::ContextualGroundingFailure,
             grounding, kGroundingThreshold, "world-perception-ledger",
             "visible world evidence lacks enough contact, support, or authored grounding");
  addFinding(report.findings, desc, BeliefFindingKind::ContactShadowCredibilityFailure,
             contact, kContactThreshold, "render-grounding",
             "contact shadow evidence is too weak for objects to read as attached to the world");
  addFinding(report.findings, desc, BeliefFindingKind::VolumetricSceneCouplingFailure,
             volumetric, kVolumetricThreshold, "lighting-atmosphere",
             "atmosphere or fog evidence is not coupled strongly enough to scene surfaces");
  addFinding(report.findings, desc, BeliefFindingKind::MaterialResponseInstability,
             material_response, kMaterialResponseThreshold, "material-response",
             "material response lacks stable memory under player-observable lighting");
  addFinding(report.findings, desc, BeliefFindingKind::LodTransitionVisibility,
             lod, kLodThreshold, "streaming-lod",
             "LOD or streaming evidence risks visible transition changes");
  addFinding(report.findings, desc, BeliefFindingKind::AssetScaleIncoherence,
             scale, kScaleThreshold, "asset-scale",
             "asset scale evidence is not coherent enough for believable placement");
  addFinding(report.findings, desc, BeliefFindingKind::EnvironmentalEntropyDeficit,
             entropy, kEntropyThreshold, "environment-history",
             "environment evidence is too uniform or clean to read as occupied over time");

  const std::array<float, 8> scores{
      material_family, grounding, contact, volumetric, material_response, lod, scale, entropy};
  float total = 0.0f;
  for (const float score : scores) {
    total += score;
  }
  report.score = clamp01(total / static_cast<float>(scores.size()));
  report.accepted = report.findings.empty() && report.score + 0.0001f >= report.minimum_score;
  report.belief_contract_hash = hashReport(desc, report);
  report.readability_audit_hash =
      mix(hashReport(desc, report), desc.perceptual_state.semantic_budget_hash);
  return report;
}

} // namespace aster
