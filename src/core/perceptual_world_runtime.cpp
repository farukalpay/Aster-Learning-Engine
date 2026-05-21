// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/perceptual_world_runtime.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace aster {
namespace {

constexpr std::uint64_t kPerceptualRuntimeSeed = 0xA57E9E9CE975E001ull;

[[nodiscard]] float clamp01(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float signal(const bool present) {
  return present ? 1.0f : 0.0f;
}

[[nodiscard]] float approach(const float current, const float target, const float rise_rate,
                             const float fall_rate, const float dt) {
  const float rate = target >= current ? rise_rate : fall_rate;
  const float alpha = 1.0f - std::exp(-std::max(rate, 0.0f) * std::max(dt, 0.0f));
  return clamp01(current + (target - current) * alpha);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kPerceptualRuntimeSeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value)));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const bool value) {
  return mix(hash, static_cast<std::uint64_t>(value ? 1u : 0u));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const Vec3 value) {
  return mix(hash, stableHash64(value));
}

[[nodiscard]] std::uint64_t mixString(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return hash;
}

[[nodiscard]] float average(const std::array<float, 7> &values) {
  float total = 0.0f;
  for (const float value : values) {
    total += value;
  }
  return total / static_cast<float>(values.size());
}

[[nodiscard]] PerceptualRenderBudget buildRenderBudget(const PerceptualFrameState &state) {
  PerceptualRenderBudget budget;
  const float confidence = clamp01(1.0f - state.continuity_debt);
  budget.lighting = clamp01(state.lighting_believability * confidence);
  budget.atmosphere = clamp01((state.occlusion_trust * 0.45f + state.lighting_believability * 0.55f) *
                              confidence);
  budget.decal = clamp01((state.material_memory * 0.55f + state.interaction_residue * 0.45f) *
                         confidence);
  budget.particle = clamp01((state.interaction_residue * 0.65f + state.ecology_signal * 0.35f) *
                            confidence);
  budget.audio = clamp01((state.player_readable_cause * 0.55f + state.ecology_signal * 0.45f) *
                         confidence);
  budget.lod_bias = clamp01(state.traversal_pressure * 0.55f + state.occlusion_trust * 0.45f);

  std::uint64_t hash = mixString(kPerceptualRuntimeSeed, "aster.perceptual.render-budget.v1");
  hash = mix(hash, budget.lighting);
  hash = mix(hash, budget.atmosphere);
  hash = mix(hash, budget.decal);
  hash = mix(hash, budget.particle);
  hash = mix(hash, budget.audio);
  hash = mix(hash, budget.lod_bias);
  budget.semantic_budget_hash = hash;
  return budget;
}

[[nodiscard]] std::uint64_t hashFrameState(const PerceptualWorldRuntimeOptions &options,
                                           const PerceptualWorldObservation &observation,
                                           const PerceptualFrameState &state) {
  std::uint64_t hash = mixString(kPerceptualRuntimeSeed, "aster.perceptual.frame-state.v1");
  hash = mixString(hash, options.id);
  hash = mix(hash, options.region_id);
  hash = mix(hash, observation.region_id);
  hash = mix(hash, observation.world_transition_hash);
  hash = mix(hash, observation.visibility_set_hash);
  hash = mix(hash, observation.player_position);
  hash = mix(hash, observation.ledger.ledger_hash);
  hash = mix(hash, observation.reaction_package_hash);
  hash = mix(hash, state.exposure_seconds);
  hash = mix(hash, state.continuity_score);
  hash = mix(hash, state.continuity_debt);
  hash = mix(hash, state.material_memory);
  hash = mix(hash, state.interaction_residue);
  hash = mix(hash, state.traversal_pressure);
  hash = mix(hash, state.lighting_believability);
  hash = mix(hash, state.occlusion_trust);
  hash = mix(hash, state.ecology_signal);
  hash = mix(hash, state.player_readable_cause);
  hash = mix(hash, state.semantic_budget_hash);
  hash = mix(hash, state.accepted);
  return hash;
}

} // namespace

PerceptualWorldRuntime::PerceptualWorldRuntime(PerceptualWorldRuntimeOptions options)
    : options_(std::move(options)) {}

void PerceptualWorldRuntime::setOptions(PerceptualWorldRuntimeOptions options) {
  options.exposure_horizon_seconds = std::max(options.exposure_horizon_seconds, 0.001f);
  options.minimum_continuity_score = clamp01(options.minimum_continuity_score);
  options.minimum_occlusion_trust = clamp01(options.minimum_occlusion_trust);
  options.minimum_lighting_believability = clamp01(options.minimum_lighting_believability);
  options.minimum_player_readable_cause = clamp01(options.minimum_player_readable_cause);
  if (options_.id != options.id || options_.region_id != options.region_id) {
    has_last_player_position_ = false;
    state_ = {};
  }
  options_ = std::move(options);
}

const PerceptualWorldRuntimeOptions &PerceptualWorldRuntime::options() const noexcept {
  return options_;
}

void PerceptualWorldRuntime::reset() {
  state_ = {};
  has_last_player_position_ = false;
}

PerceptualFrameState
PerceptualWorldRuntime::advance(const PerceptualWorldObservation &observation) {
  const float dt = std::clamp(observation.delta_seconds, 0.0f, 1.0f);
  const std::uint64_t observation_region =
      observation.region_id != 0u ? observation.region_id : options_.region_id;
  if (observation_region != 0u && options_.region_id != 0u &&
      observation_region != options_.region_id) {
    has_last_player_position_ = false;
    state_ = {};
  }

  const float measured_speed =
      has_last_player_position_ && dt > 0.0f
          ? length(observation.player_position - last_player_position_) / std::max(dt, 0.001f)
          : 0.0f;
  const float traversal_speed =
      std::max(std::max(observation.traversal_speed, measured_speed), 0.0f);
  last_player_position_ = observation.player_position;
  has_last_player_position_ = true;

  const WorldPerceptionLedgerReport &ledger = observation.ledger;
  const float salience = clamp01(observation.perceptual_salience_score);
  const float material_signal =
      clamp01(signal(ledger.material_memory_hash != 0u) * 0.30f +
              signal(ledger.wear_continuity_hash != 0u) * 0.20f +
              signal(observation.material_memory_hash != 0u) * 0.20f +
              signal(observation.resource_state_hash != 0u) * 0.15f + salience * 0.15f);
  const float interaction_signal =
      clamp01(signal(ledger.contact_history_hash != 0u) * 0.18f +
              signal(observation.contact_history_hash != 0u) * 0.12f +
              signal(observation.event_residue_hash != 0u) * 0.24f +
              signal(observation.reaction_package_hash != 0u) * 0.22f +
              signal(observation.audio_visual_cue_budget_hash != 0u) * 0.14f +
              signal(observation.readability_audit_hash != 0u) * 0.10f);
  const float traversal_signal =
      clamp01(signal(observation.navigation_valid) * 0.30f +
              signal(ledger.streaming_semantic_lod_hash != 0u ||
                     observation.streaming_residency_lod_hash != 0u) *
                  0.24f +
              clamp01(traversal_speed / 3.0f) * 0.28f + salience * 0.18f);
  const float lighting_signal =
      clamp01(signal(ledger.lighting_exposure_hash != 0u ||
                     observation.lighting_atmosphere_hash != 0u) *
                  0.36f +
              signal(ledger.atmosphere_cell_hash != 0u) * 0.24f +
              signal(ledger.audio_visual_cue_budget_hash != 0u ||
                     observation.audio_visual_cue_budget_hash != 0u) *
                  0.16f +
              salience * 0.24f);
  const float occlusion_signal =
      clamp01(signal(ledger.occlusion_role_hash != 0u) * 0.38f +
              signal(observation.visibility_set_hash != 0u) * 0.20f +
              signal(ledger.contact_history_hash != 0u) * 0.12f +
              signal(ledger.streaming_semantic_lod_hash != 0u) * 0.15f +
              signal(observation.navigation_valid) * 0.15f);
  const float ecology_signal =
      clamp01(signal(ledger.gameplay_affordance_hash != 0u) * 0.22f +
              signal(observation.resource_state_hash != 0u ||
                     ledger.material_memory_hash != 0u) *
                  0.18f +
              signal(observation.ai_attention_hash != 0u) * 0.18f +
              clamp01(observation.encounter_pressure) * 0.16f +
              clamp01(observation.resource_pressure) * 0.14f +
              signal(observation.event_residue_hash != 0u) * 0.12f);
  const float cause_signal =
      clamp01(signal(observation.reaction_package_hash != 0u) * 0.24f +
              signal(observation.event_residue_hash != 0u) * 0.22f +
              signal(observation.readability_audit_hash != 0u) * 0.18f +
              signal(ledger.audio_visual_cue_budget_hash != 0u ||
                     observation.audio_visual_cue_budget_hash != 0u) *
                  0.14f +
              signal(ledger.gameplay_affordance_hash != 0u) * 0.12f +
              signal(ledger.material_memory_hash != 0u) * 0.08f +
              signal(ledger.lighting_exposure_hash != 0u || ledger.atmosphere_cell_hash != 0u) *
                  0.08f +
              signal(ledger.streaming_semantic_lod_hash != 0u) * 0.06f +
              clamp01(observation.explicit_player_readable_cause) * 0.14f);

  state_.exposure_seconds = std::max(0.0f, state_.exposure_seconds + dt);
  state_.material_memory = approach(state_.material_memory, material_signal, 1.35f, 0.040f, dt);
  state_.interaction_residue =
      approach(state_.interaction_residue, interaction_signal, 1.80f, 0.030f, dt);
  state_.traversal_pressure =
      approach(state_.traversal_pressure, traversal_signal, 1.20f, 0.055f, dt);
  state_.lighting_believability =
      approach(state_.lighting_believability, lighting_signal, 1.10f, 0.045f, dt);
  state_.occlusion_trust = approach(state_.occlusion_trust, occlusion_signal, 1.00f, 0.025f, dt);
  state_.ecology_signal = approach(state_.ecology_signal, ecology_signal, 1.05f, 0.035f, dt);
  state_.player_readable_cause =
      approach(state_.player_readable_cause, cause_signal, 1.45f, 0.030f, dt);

  const float exposure_progress =
      clamp01(state_.exposure_seconds / std::max(options_.exposure_horizon_seconds, 0.001f));
  const float raw_continuity =
      average({state_.material_memory,
               state_.interaction_residue,
               state_.traversal_pressure,
               state_.lighting_believability,
               state_.occlusion_trust,
               state_.ecology_signal,
               state_.player_readable_cause});
  state_.continuity_score = clamp01(raw_continuity * (0.55f + exposure_progress * 0.45f));
  const float target_debt =
      clamp01(std::max(0.0f, options_.minimum_continuity_score - state_.continuity_score) +
              std::max(0.0f, options_.minimum_occlusion_trust - state_.occlusion_trust) * 0.35f +
              std::max(0.0f,
                       options_.minimum_lighting_believability - state_.lighting_believability) *
                  0.35f +
              std::max(0.0f, options_.minimum_player_readable_cause -
                                 state_.player_readable_cause) *
                  0.35f +
              (1.0f - exposure_progress) * 0.18f);
  state_.continuity_debt = approach(state_.continuity_debt, target_debt, 1.50f, 0.70f, dt);
  state_.render_budget = buildRenderBudget(state_);
  state_.semantic_budget_hash = state_.render_budget.semantic_budget_hash;
  state_.accepted = state_.continuity_score + 0.0001f >= options_.minimum_continuity_score &&
                    state_.occlusion_trust + 0.0001f >= options_.minimum_occlusion_trust &&
                    state_.lighting_believability + 0.0001f >=
                        options_.minimum_lighting_believability &&
                    state_.player_readable_cause + 0.0001f >=
                        options_.minimum_player_readable_cause &&
                    exposure_progress + 0.0001f >= 1.0f;
  state_.diagnostic =
      state_.accepted ? "perceptual world runtime accepted"
                      : "perceptual world runtime accumulating continuity evidence";
  state_.perceptual_state_hash = hashFrameState(options_, observation, state_);
  return state_;
}

const PerceptualFrameState &PerceptualWorldRuntime::lastState() const noexcept {
  return state_;
}

PerceptualWorldObservation
makePerceptualWorldObservation(const WorldPerceptionLedgerReport &ledger) {
  PerceptualWorldObservation observation;
  observation.region_id = ledger.region_id;
  observation.ledger = ledger;
  observation.perceptual_salience_score = ledger.score;
  return observation;
}

} // namespace aster
