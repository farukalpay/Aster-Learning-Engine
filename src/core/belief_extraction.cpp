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

[[nodiscard]] float signal(const bool present) {
  return present ? 1.0f : 0.0f;
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

[[nodiscard]] float backendVisualTruthScore(const BeliefExtractionDesc &desc) {
  if (!desc.backend_visual_truth_required) {
    return 1.0f;
  }
  const std::array<float, 6> scores{scoreWithDefault(desc.backend_visual_truth_score),
                                   signal(desc.backend_hdr_equivalent),
                                   signal(desc.backend_msaa_equivalent),
                                   signal(desc.backend_timestamp_equivalent),
                                   signal(desc.backend_swapchain_equivalent),
                                   signal(desc.backend_fog_probe_shadow_equivalent)};
  float total = 0.0f;
  for (const float score : scores) {
    total += score;
  }
  return clamp01(total / static_cast<float>(scores.size()));
}

[[nodiscard]] float lightHistoryScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.light_history_continuity);
  if (desc.perceptual_primitive_summary.primitive_count > 0u) {
    score = std::min(score, scoreWithDefault(desc.perceptual_primitive_summary.light_history));
  }
  return score;
}

[[nodiscard]] float interactionDebtScore(const BeliefExtractionDesc &desc) {
  float score = 1.0f - scoreWithDefault(desc.interaction_debt_leak);
  if (desc.perceptual_primitive_summary.primitive_count > 0u) {
    const float residue = scoreWithDefault(desc.perceptual_primitive_summary.interaction_residue);
    const float debt = scoreWithDefault(desc.perceptual_primitive_summary.perceptual_debt);
    score = std::min(score, residue > 0.05f ? 1.0f - debt * 0.45f : 0.45f);
  }
  return clamp01(score);
}

[[nodiscard]] float semanticRepetitionGroundingScore(const BeliefExtractionDesc &desc) {
  float score = 1.0f - scoreWithDefault(desc.semantic_repetition_score);
  if (desc.perceptual_primitive_summary.primitive_count > 0u) {
    score = std::max(score, scoreWithDefault(desc.perceptual_primitive_summary.semantic_lod));
  }
  return clamp01(score);
}

[[nodiscard]] float aiAttentionScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.ai_attention_coherence);
  if (desc.perceptual_primitive_summary.primitive_count > 0u &&
      desc.perceptual_primitive_summary.threat_gradient > 0.0f) {
    score = std::min(score,
                     std::max(scoreWithDefault(desc.perceptual_primitive_summary.decision_impact),
                              0.50f));
  }
  return score;
}

[[nodiscard]] float surfaceMemoryScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.surface_memory_continuity);
  if (desc.perceptual_primitive_summary.primitive_count > 0u) {
    score = std::min(score,
                     std::max(scoreWithDefault(desc.perceptual_primitive_summary.material_memory),
                              0.35f));
  }
  return score;
}

[[nodiscard]] float acousticTruthScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.acoustic_truth);
  if (desc.perceptual_primitive_summary.primitive_count > 0u) {
    const float primitive_acoustic =
        scoreWithDefault(desc.perceptual_primitive_summary.acoustic_occlusion);
    score = std::min(score, primitive_acoustic > 0.0f ? 0.82f : 1.0f);
  }
  return score;
}

[[nodiscard]] float worldStateSyncScore(const BeliefExtractionDesc &desc) {
  float score = scoreWithDefault(desc.world_state_sync);
  if (desc.perceptual_primitive_summary.primitive_count > 0u &&
      desc.perceptual_primitive_summary.truth_hash == 0u) {
    score = 0.0f;
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
  hash = mix(hash, desc.perceptual_primitive_summary.truth_hash);
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
  hash = mix(hash, desc.perceptual_primitive_summary.truth_hash);
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
  case BeliefFindingKind::BackendVisualTruthGap:
    return "backend_visual_truth_gap";
  case BeliefFindingKind::LightHistoryDiscontinuity:
    return "light_history_discontinuity";
  case BeliefFindingKind::InteractionDebtLeak:
    return "interaction_debt_leak";
  case BeliefFindingKind::SemanticRepetition:
    return "semantic_repetition";
  case BeliefFindingKind::AiAttentionIncoherence:
    return "ai_attention_incoherence";
  case BeliefFindingKind::SurfaceMemoryReset:
    return "surface_memory_reset";
  case BeliefFindingKind::AcousticFalseness:
    return "acoustic_falseness";
  case BeliefFindingKind::WorldStateDesynchronization:
    return "world_state_desynchronization";
  case BeliefFindingKind::MissingPerceptualPrimitive:
    return "missing_perceptual_primitive";
  case BeliefFindingKind::UnresolvedPerceptualBinding:
    return "unresolved_perceptual_binding";
  case BeliefFindingKind::PerceptualExtractionDesynchronization:
    return "perceptual_extraction_desynchronization";
  case BeliefFindingKind::BackendPerceptualTruthGap:
    return "backend_perceptual_truth_gap";
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
  constexpr float kBackendTruthThreshold = 0.74f;
  constexpr float kLightHistoryThreshold = 0.60f;
  constexpr float kInteractionDebtThreshold = 0.58f;
  constexpr float kSemanticThreshold = 0.55f;
  constexpr float kAiAttentionThreshold = 0.58f;
  constexpr float kSurfaceMemoryThreshold = 0.60f;
  constexpr float kAcousticTruthThreshold = 0.58f;
  constexpr float kWorldSyncThreshold = 0.70f;
  constexpr float kPrimitiveCoverageThreshold = 1.0f;
  constexpr float kBindingThreshold = 1.0f;
  constexpr float kExtractionSyncThreshold = 1.0f;
  constexpr float kBackendPrimitiveThreshold = 0.74f;

  const float material_family = materialFamilyUniqueness(desc);
  const float grounding = contextualGroundingScore(desc);
  const float contact = contactShadowScore(desc);
  const float volumetric = volumetricCouplingScore(desc);
  const float material_response = materialResponseScore(desc);
  const float lod = scoreWithDefault(desc.lod_transition_invisibility);
  const float scale = scoreWithDefault(desc.asset_scale_coherence);
  const float entropy = environmentalEntropyScore(desc);
  const float backend_truth = backendVisualTruthScore(desc);
  const float light_history = lightHistoryScore(desc);
  const float interaction_debt = interactionDebtScore(desc);
  const float semantic_repetition = semanticRepetitionGroundingScore(desc);
  const float ai_attention = aiAttentionScore(desc);
  const float surface_memory = surfaceMemoryScore(desc);
  const float acoustic_truth = acousticTruthScore(desc);
  const float world_sync = worldStateSyncScore(desc);
  const bool has_primitive_summary = desc.perceptual_primitive_summary.primitive_count > 0u;
  const float primitive_coverage =
      has_primitive_summary && desc.visible_object_count > 0u
          ? clamp01(static_cast<float>(desc.perceptual_primitive_summary.primitive_count) /
                    static_cast<float>(desc.visible_object_count))
          : 1.0f;
  const bool has_subrecords =
      desc.perceptual_primitive_summary.active_cell_anchor_count >=
          desc.perceptual_primitive_summary.primitive_count &&
      desc.perceptual_primitive_summary.active_surface_patch_count >=
          desc.perceptual_primitive_summary.primitive_count &&
      desc.perceptual_primitive_summary.active_contact_zone_count >=
          desc.perceptual_primitive_summary.primitive_count &&
      desc.perceptual_primitive_summary.active_residue_channel_count >=
          desc.perceptual_primitive_summary.primitive_count;
  const float binding_score = has_primitive_summary ? signal(has_subrecords) : 1.0f;
  const float extraction_sync =
      has_primitive_summary ? signal(desc.perceptual_primitive_summary.truth_hash != 0u) : 1.0f;
  const float backend_primitive_truth =
      has_primitive_summary && desc.backend_visual_truth_required
          ? backend_truth
          : 1.0f;

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
  addFinding(report.findings, desc, BeliefFindingKind::BackendVisualTruthGap,
             backend_truth, kBackendTruthThreshold, "backend-visual-truth",
             "backend proof does not establish equivalent HDR, MSAA, timestamp, swapchain, fog, "
             "probe, and shadow truth");
  addFinding(report.findings, desc, BeliefFindingKind::LightHistoryDiscontinuity,
             light_history, kLightHistoryThreshold, "light-history-cache",
             "light exposure history is discontinuous across world state, fog, probes, or surfaces");
  addFinding(report.findings, desc, BeliefFindingKind::InteractionDebtLeak,
             interaction_debt, kInteractionDebtThreshold, "perceptual-debt",
             "interaction debt is not being paid back into persistent material or residue state");
  addFinding(report.findings, desc, BeliefFindingKind::SemanticRepetition,
             semantic_repetition, kSemanticThreshold, "semantic-lod",
             "semantic LOD or material usage repeats too strongly for the authored context");
  addFinding(report.findings, desc, BeliefFindingKind::AiAttentionIncoherence,
             ai_attention, kAiAttentionThreshold, "ai-visibility",
             "AI attention evidence does not match threat, cover, or player-readable causes");
  addFinding(report.findings, desc, BeliefFindingKind::SurfaceMemoryReset,
             surface_memory, kSurfaceMemoryThreshold, "surface-memory",
             "surface memory appears reset instead of preserving accumulated contact and material history");
  addFinding(report.findings, desc, BeliefFindingKind::AcousticFalseness,
             acoustic_truth, kAcousticTruthThreshold, "acoustic-occlusion",
             "audio cue budget or acoustic occlusion evidence does not match the world state");
  addFinding(report.findings, desc, BeliefFindingKind::WorldStateDesynchronization,
             world_sync, kWorldSyncThreshold, "world-truth-audit",
             "world, belief, render extraction, or perceptual primitive hashes are desynchronized");
  addFinding(report.findings, desc, BeliefFindingKind::MissingPerceptualPrimitive,
             primitive_coverage, kPrimitiveCoverageThreshold, "world-perceptual-primitive",
             "strict renderables are missing required perceptual primitive truth");
  addFinding(report.findings, desc, BeliefFindingKind::UnresolvedPerceptualBinding,
             binding_score, kBindingThreshold, "perceptual-authoring-binding",
             "perceptual template or placement binding did not resolve into all primitive subrecords");
  addFinding(report.findings, desc, BeliefFindingKind::PerceptualExtractionDesynchronization,
             extraction_sync, kExtractionSyncThreshold, "render-extraction",
             "perceptual primitive truth is not synchronized with extraction evidence");
  addFinding(report.findings, desc, BeliefFindingKind::BackendPerceptualTruthGap,
             backend_primitive_truth, kBackendPrimitiveThreshold, "backend-perceptual-truth",
             "backend did not prove equivalent consumption of perceptual primitive payloads");

  const std::array<float, 20> scores{material_family,
                                     grounding,
                                     contact,
                                     volumetric,
                                     material_response,
                                     lod,
                                     scale,
                                     entropy,
                                     backend_truth,
                                     light_history,
                                     interaction_debt,
                                     semantic_repetition,
                                     ai_attention,
                                     surface_memory,
                                     acoustic_truth,
                                     world_sync,
                                     primitive_coverage,
                                     binding_score,
                                     extraction_sync,
                                     backend_primitive_truth};
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
