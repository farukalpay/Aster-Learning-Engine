// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/perceptual_world_runtime.hpp"
#include "aster/core/world_perception_ledger.hpp"
#include "aster/core/world_perceptual_primitive.hpp"
#include "aster/scene/scene_coherence.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class BeliefFindingKind {
  MaterialFamilyCollapse,
  ContextualGroundingFailure,
  ContactShadowCredibilityFailure,
  VolumetricSceneCouplingFailure,
  MaterialResponseInstability,
  LodTransitionVisibility,
  AssetScaleIncoherence,
  EnvironmentalEntropyDeficit,
  BackendVisualTruthGap,
  LightHistoryDiscontinuity,
  InteractionDebtLeak,
  SemanticRepetition,
  AiAttentionIncoherence,
  SurfaceMemoryReset,
  AcousticFalseness,
  WorldStateDesynchronization,
  MissingPerceptualPrimitive,
  UnresolvedPerceptualBinding,
  PerceptualExtractionDesynchronization,
  BackendPerceptualTruthGap,
};

enum class BeliefFindingSeverity {
  Info,
  Warning,
  Error,
};

struct BeliefExtractionFinding {
  BeliefFindingKind kind = BeliefFindingKind::MaterialFamilyCollapse;
  BeliefFindingSeverity severity = BeliefFindingSeverity::Warning;
  std::string subject;
  float score = 1.0f;
  float threshold = 1.0f;
  std::uint64_t evidence_hash = 0u;
  std::string source;
  std::string message;
};

struct BeliefExtractionDesc {
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t extraction_hash = 0u;
  std::uint64_t source_hash = 0u;
  std::string subject = "world";
  float minimum_score = 0.70f;
  WorldPerceptionLedgerReport perception_ledger;
  PerceptualFrameState perceptual_state;
  SceneCoherenceReport scene_coherence;
  bool has_scene_coherence = false;
  std::size_t visible_object_count = 0u;
  std::size_t material_family_count = 0u;
  bool contact_shadow_required = true;
  bool contact_shadow_enabled = true;
  bool volumetric_required = false;
  bool volumetric_scene_coupled = true;
  float material_response_stability = 1.0f;
  float contact_shadow_credibility = 1.0f;
  float volumetric_scene_coupling = 1.0f;
  float lod_transition_invisibility = 1.0f;
  float asset_scale_coherence = 1.0f;
  float environmental_entropy = 1.0f;
  bool backend_visual_truth_required = false;
  bool backend_hdr_equivalent = true;
  bool backend_msaa_equivalent = true;
  bool backend_timestamp_equivalent = true;
  bool backend_swapchain_equivalent = true;
  bool backend_fog_probe_shadow_equivalent = true;
  float backend_visual_truth_score = 1.0f;
  float light_history_continuity = 1.0f;
  float interaction_debt_leak = 0.0f;
  float semantic_repetition_score = 0.0f;
  float ai_attention_coherence = 1.0f;
  float surface_memory_continuity = 1.0f;
  float acoustic_truth = 1.0f;
  float world_state_sync = 1.0f;
  WorldPerceptualPrimitiveSummary perceptual_primitive_summary;
};

struct BeliefExtractionReport {
  bool accepted = false;
  float score = 0.0f;
  float minimum_score = 0.70f;
  std::uint64_t belief_contract_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;
  std::vector<BeliefExtractionFinding> findings;
};

[[nodiscard]] std::string_view beliefFindingKindName(BeliefFindingKind kind) noexcept;
[[nodiscard]] std::string_view beliefFindingSeverityName(BeliefFindingSeverity severity) noexcept;
[[nodiscard]] inline bool isCanonicalBeliefFindingName(const std::string_view name) noexcept {
  return name == "material_family_collapse" || name == "contextual_grounding_failure" ||
         name == "contact_shadow_credibility_failure" ||
         name == "volumetric_scene_coupling_failure" ||
         name == "material_response_instability" ||
         name == "lod_transition_visibility" || name == "asset_scale_incoherence" ||
         name == "environmental_entropy_deficit" || name == "backend_visual_truth_gap" ||
         name == "light_history_discontinuity" || name == "interaction_debt_leak" ||
         name == "semantic_repetition" || name == "ai_attention_incoherence" ||
         name == "surface_memory_reset" || name == "acoustic_falseness" ||
         name == "world_state_desynchronization" || name == "missing_perceptual_primitive" ||
         name == "unresolved_perceptual_binding" ||
         name == "perceptual_extraction_desynchronization" ||
         name == "backend_perceptual_truth_gap";
}
[[nodiscard]] BeliefExtractionReport
extractBeliefContract(const BeliefExtractionDesc &desc);

} // namespace aster
