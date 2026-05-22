// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/world_perception_ledger.hpp"
#include "aster/math/vec.hpp"

#include <cstdint>
#include <string>

namespace aster {

struct PerceptualWorldRuntimeOptions {
  std::uint64_t region_id = 0u;
  std::string id = "perceptual_world_runtime";
  float exposure_horizon_seconds = 47.0f;
  float minimum_continuity_score = 0.62f;
  float minimum_occlusion_trust = 0.45f;
  float minimum_lighting_believability = 0.45f;
  float minimum_player_readable_cause = 0.45f;
};

struct PerceptualWorldObservation {
  float delta_seconds = 1.0f / 60.0f;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  std::uint64_t region_id = 0u;
  Vec3 player_position{};
  bool navigation_valid = false;
  float perceptual_salience_score = 0.0f;
  float traversal_speed = 0.0f;
  float encounter_pressure = 0.0f;
  float resource_pressure = 0.0f;
  float explicit_player_readable_cause = 0.0f;
  WorldPerceptionLedgerReport ledger;
  std::uint64_t reaction_package_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t contact_history_hash = 0u;
  std::uint64_t lighting_atmosphere_hash = 0u;
  std::uint64_t wear_continuity_hash = 0u;
  std::uint64_t ai_attention_hash = 0u;
  std::uint64_t streaming_residency_lod_hash = 0u;
  std::uint64_t resource_state_hash = 0u;
  std::uint64_t event_residue_hash = 0u;
  std::uint64_t audio_visual_cue_budget_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;
};

struct PerceptualRenderBudget {
  std::uint64_t semantic_budget_hash = 0u;
  float lighting = 0.0f;
  float atmosphere = 0.0f;
  float decal = 0.0f;
  float particle = 0.0f;
  float audio = 0.0f;
  float lod_bias = 0.0f;
};

struct PerceptualFrameState {
  std::uint64_t perceptual_state_hash = 0u;
  float continuity_debt = 1.0f;
  float material_memory = 0.0f;
  float interaction_residue = 0.0f;
  float traversal_pressure = 0.0f;
  float lighting_believability = 0.0f;
  float occlusion_trust = 0.0f;
  float ecology_signal = 0.0f;
  float player_readable_cause = 0.0f;
  std::uint64_t semantic_budget_hash = 0u;
  bool accepted = false;
  std::string diagnostic = "perceptual world runtime has not advanced";
  float exposure_seconds = 0.0f;
  float continuity_score = 0.0f;
  PerceptualRenderBudget render_budget;
};

struct PerceptualWorldScheduleDesc {
  std::uint64_t region_id = 0u;
  std::uint64_t world_transition_hash = 0u;
  std::uint64_t actor_state_delta_hash = 0u;
  std::uint64_t sensory_event_hash = 0u;
  std::uint64_t visibility_set_hash = 0u;
  bool navigation_valid = false;
  float perceptual_salience_score = 0.0f;
  float encounter_pressure = 0.0f;
  float resource_pressure = 0.0f;
  float frame_cost_ms = 0.0f;
  WorldPerceptionLedgerReport ledger;
  PerceptualFrameState perceptual_state;
  std::uint64_t reaction_package_hash = 0u;
  std::uint64_t material_memory_hash = 0u;
  std::uint64_t contact_history_hash = 0u;
  std::uint64_t lighting_atmosphere_hash = 0u;
  std::uint64_t wear_continuity_hash = 0u;
  std::uint64_t ai_attention_hash = 0u;
  std::uint64_t streaming_residency_lod_hash = 0u;
  std::uint64_t resource_state_hash = 0u;
  std::uint64_t event_residue_hash = 0u;
  std::uint64_t audio_visual_cue_budget_hash = 0u;
  std::uint64_t readability_audit_hash = 0u;
  float minimum_belief_stability = 0.58f;
};

struct PerceptualWorldScheduleReport {
  bool accepted = false;
  std::uint64_t scheduler_hash = 0u;
  std::uint64_t memory_residue_hash = 0u;
  std::uint64_t threat_signal_hash = 0u;
  std::uint64_t material_age_hash = 0u;
  std::uint64_t interaction_debt_hash = 0u;
  std::uint64_t perceptual_priority_hash = 0u;
  std::uint64_t streaming_budget_hash = 0u;
  float memory_residue = 0.0f;
  float threat_signal = 0.0f;
  float material_age = 0.0f;
  float interaction_debt = 0.0f;
  float perceptual_priority = 0.0f;
  float streaming_budget = 0.0f;
  float belief_stability = 0.0f;
  float decision_impact_score = 0.0f;
  float frame_cost_ms = 0.0f;
  std::string diagnostic = "perceptual world scheduler has not run";
};

class PerceptualWorldRuntime {
public:
  PerceptualWorldRuntime() = default;
  explicit PerceptualWorldRuntime(PerceptualWorldRuntimeOptions options);

  void setOptions(PerceptualWorldRuntimeOptions options);
  [[nodiscard]] const PerceptualWorldRuntimeOptions &options() const noexcept;
  void reset();
  [[nodiscard]] PerceptualFrameState advance(const PerceptualWorldObservation &observation);
  [[nodiscard]] const PerceptualFrameState &lastState() const noexcept;

private:
  PerceptualWorldRuntimeOptions options_{};
  PerceptualFrameState state_{};
  Vec3 last_player_position_{};
  bool has_last_player_position_ = false;
};

[[nodiscard]] PerceptualWorldObservation
makePerceptualWorldObservation(const WorldPerceptionLedgerReport &ledger);

[[nodiscard]] PerceptualWorldScheduleReport
schedulePerceptualWorld(const PerceptualWorldScheduleDesc &desc);

} // namespace aster
