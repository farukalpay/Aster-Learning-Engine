// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/world_perceptual_primitive.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace aster {
namespace {

constexpr std::uint64_t kWorldPrimitiveSeed = 0xA57E901D9E9CE001ull;

[[nodiscard]] float clamp01(const float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] float finite01(const float value) {
  return std::isfinite(value) ? clamp01(value) : 0.0f;
}

[[nodiscard]] float halfLifeApproach(const float current, const float target,
                                     const float delta_seconds,
                                     const float half_life_seconds) {
  const float half_life = std::max(half_life_seconds, 0.001f);
  const float alpha = 1.0f - std::pow(0.5f, std::max(delta_seconds, 0.0f) / half_life);
  return finite01(current + (target - current) * alpha);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kWorldPrimitiveSeed : hash, value);
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
  return mix(hash, static_cast<std::uint64_t>(value.size()));
}

[[nodiscard]] bool active(const float value) {
  return finite01(value) > 0.0001f;
}

[[nodiscard]] WorldPerceptualSignals normalized(WorldPerceptualSignals signals) {
  signals.belief_state = finite01(signals.belief_state);
  signals.perceptual_debt = finite01(signals.perceptual_debt);
  signals.material_memory = finite01(signals.material_memory);
  signals.interaction_residue = finite01(signals.interaction_residue);
  signals.contact_field = finite01(signals.contact_field);
  signals.light_history = finite01(signals.light_history);
  signals.acoustic_occlusion = finite01(signals.acoustic_occlusion);
  signals.ecology_pressure = finite01(signals.ecology_pressure);
  signals.threat_gradient = finite01(signals.threat_gradient);
  signals.traversal_pressure = finite01(signals.traversal_pressure);
  signals.semantic_lod = finite01(signals.semantic_lod);
  signals.decision_impact = finite01(signals.decision_impact);
  signals.player_readable_cause = finite01(signals.player_readable_cause);
  return signals;
}

void mixSignals(std::uint64_t &hash, const WorldPerceptualSignals &signals) {
  hash = mix(hash, signals.belief_state);
  hash = mix(hash, signals.perceptual_debt);
  hash = mix(hash, signals.material_memory);
  hash = mix(hash, signals.interaction_residue);
  hash = mix(hash, signals.contact_field);
  hash = mix(hash, signals.light_history);
  hash = mix(hash, signals.acoustic_occlusion);
  hash = mix(hash, signals.ecology_pressure);
  hash = mix(hash, signals.threat_gradient);
  hash = mix(hash, signals.traversal_pressure);
  hash = mix(hash, signals.semantic_lod);
  hash = mix(hash, signals.decision_impact);
  hash = mix(hash, signals.player_readable_cause);
}

[[nodiscard]] std::uint64_t hashPrimitive(const WorldPerceptualPrimitiveDesc &desc,
                                          const WorldPerceptualPrimitive &primitive) {
  std::uint64_t hash = mixString(kWorldPrimitiveSeed, "aster.world-perceptual-primitive.v1");
  hash = mixString(hash, primitive.primitive_id);
  hash = mixString(hash, primitive.object_name);
  hash = mix(hash, primitive.world_owner_hash);
  hash = mix(hash, primitive.template_hash);
  hash = mix(hash, primitive.cell_hash);
  hash = mix(hash, primitive.player_readable_cause_hash);
  hash = mix(hash, primitive.sound_surface_class_hash);
  hash = mix(hash, primitive.neural_irradiance_hash);
  hash = mix(hash, primitive.cell_residency);
  hash = mix(hash, primitive.world_ownership);
  hash = mix(hash, primitive.wetness_half_life_seconds);
  hash = mix(hash, primitive.material_half_life_seconds);
  hash = mix(hash, primitive.exposure_age_seconds);
  hash = mix(hash, primitive.streaming_cost);
  hash = mix(hash, primitive.material_stability);
  hash = mix(hash, primitive.contact_normal_history);
  hash = mix(hash, primitive.acoustic_occlusion_trust);
  hash = mix(hash, primitive.visual_occlusion_trust);
  hash = mix(hash, primitive.ai_cover_value);
  hash = mix(hash, primitive.traversal_affordance);
  hash = mix(hash, primitive.semantic_lod);
  hash = mix(hash, primitive.neural_irradiance);
  hash = mix(hash, primitive.neural_irradiance_confidence);
  hash = mix(hash, desc.player_observable);
  mixSignals(hash, primitive.signals);
  for (const WorldPerceptualCellAnchor &anchor : primitive.cell_anchors) {
    hash = mixString(hash, anchor.id);
    hash = mix(hash, anchor.cell_hash);
    hash = mix(hash, anchor.center);
    hash = mix(hash, finite01(anchor.residency));
    hash = mix(hash, finite01(anchor.streaming_cost));
  }
  for (const WorldPerceptualSurfacePatch &patch : primitive.surface_patches) {
    hash = mixString(hash, patch.id);
    hash = mix(hash, patch.patch_hash);
    hash = mix(hash, patch.normal);
    hash = mix(hash, finite01(patch.wetness_flow));
    hash = mix(hash, finite01(patch.exposure_age));
    hash = mix(hash, finite01(patch.thermal_history));
    hash = mix(hash, finite01(patch.chemical_history));
    hash = mix(hash, finite01(patch.material_stability));
  }
  for (const WorldPerceptualContactZone &zone : primitive.contact_zones) {
    hash = mixString(hash, zone.id);
    hash = mix(hash, zone.zone_hash);
    hash = mix(hash, zone.normal);
    hash = mix(hash, finite01(zone.contact_field));
    hash = mix(hash, finite01(zone.occlusion_trust));
    hash = mix(hash, finite01(zone.ai_cover_value));
    hash = mix(hash, finite01(zone.traversal_affordance));
  }
  for (const WorldPerceptualResidueChannel &channel : primitive.residue_channels) {
    hash = mixString(hash, channel.id);
    hash = mix(hash, channel.channel_hash);
    hash = mix(hash, finite01(channel.residue));
    hash = mix(hash, finite01(channel.acoustic_occlusion));
    hash = mix(hash, finite01(channel.ecology_signal));
    hash = mix(hash, finite01(channel.threat));
    hash = mix(hash, finite01(channel.decision_impact));
  }
  return hash;
}

[[nodiscard]] float averageSignals(const WorldPerceptualSignals &signals) {
  const std::array<float, 12> values{signals.belief_state,
                                    1.0f - signals.perceptual_debt,
                                    signals.material_memory,
                                    signals.interaction_residue,
                                    signals.contact_field,
                                    signals.light_history,
                                    1.0f - signals.acoustic_occlusion,
                                    signals.ecology_pressure,
                                    signals.threat_gradient,
                                    signals.traversal_pressure,
                                    signals.semantic_lod,
                                    signals.player_readable_cause};
  float total = 0.0f;
  for (const float value : values) {
    total += finite01(value);
  }
  return total / static_cast<float>(values.size());
}

} // namespace

WorldPerceptualSignals decayWorldPerceptualSignals(const WorldPerceptualSignals &previous,
                                                   const WorldPerceptualSignals &target,
                                                   const float delta_seconds,
                                                   const float wetness_half_life_seconds) {
  const WorldPerceptualSignals p = normalized(previous);
  const WorldPerceptualSignals t = normalized(target);
  const float fast_half_life = std::max(wetness_half_life_seconds * 0.35f, 0.001f);
  const float slow_half_life = std::max(wetness_half_life_seconds, 0.001f);
  WorldPerceptualSignals out;
  out.belief_state = halfLifeApproach(p.belief_state, t.belief_state, delta_seconds, fast_half_life);
  out.perceptual_debt =
      halfLifeApproach(p.perceptual_debt, t.perceptual_debt, delta_seconds, fast_half_life);
  out.material_memory =
      halfLifeApproach(p.material_memory, t.material_memory, delta_seconds, slow_half_life);
  out.interaction_residue = halfLifeApproach(p.interaction_residue, t.interaction_residue,
                                             delta_seconds, slow_half_life);
  out.contact_field =
      halfLifeApproach(p.contact_field, t.contact_field, delta_seconds, fast_half_life);
  out.light_history =
      halfLifeApproach(p.light_history, t.light_history, delta_seconds, slow_half_life);
  out.acoustic_occlusion = halfLifeApproach(p.acoustic_occlusion, t.acoustic_occlusion,
                                            delta_seconds, slow_half_life);
  out.ecology_pressure =
      halfLifeApproach(p.ecology_pressure, t.ecology_pressure, delta_seconds, slow_half_life);
  out.threat_gradient =
      halfLifeApproach(p.threat_gradient, t.threat_gradient, delta_seconds, fast_half_life);
  out.traversal_pressure = halfLifeApproach(p.traversal_pressure, t.traversal_pressure,
                                            delta_seconds, fast_half_life);
  out.semantic_lod =
      halfLifeApproach(p.semantic_lod, t.semantic_lod, delta_seconds, fast_half_life);
  out.decision_impact =
      halfLifeApproach(p.decision_impact, t.decision_impact, delta_seconds, fast_half_life);
  out.player_readable_cause = halfLifeApproach(p.player_readable_cause, t.player_readable_cause,
                                               delta_seconds, fast_half_life);
  return out;
}

WorldPerceptualPrimitive evaluateWorldPerceptualPrimitive(
    const WorldPerceptualPrimitiveDesc &desc) {
  WorldPerceptualPrimitive primitive;
  primitive.primitive_id = desc.primitive_id;
  primitive.object_name = desc.object_name;
  primitive.world_owner_hash = desc.world_owner_hash;
  primitive.template_hash = desc.template_hash;
  primitive.cell_hash = desc.cell_hash;
  primitive.player_readable_cause_hash = desc.player_readable_cause_hash;
  primitive.sound_surface_class_hash = desc.sound_surface_class_hash;
  primitive.neural_irradiance_hash = desc.neural_irradiance_hash;
  primitive.cell_residency = finite01(desc.cell_residency);
  primitive.world_ownership = finite01(desc.world_ownership);
  primitive.wetness_half_life_seconds = std::max(desc.wetness_half_life_seconds, 0.001f);
  primitive.material_half_life_seconds = std::max(desc.material_half_life_seconds, 0.001f);
  primitive.exposure_age_seconds = std::max(desc.exposure_age_seconds, 0.0f);
  primitive.streaming_cost = finite01(desc.streaming_cost);
  primitive.material_stability = finite01(desc.material_stability);
  primitive.contact_normal_history = normalizeOr(desc.contact_normal_history, {0.0f, 1.0f, 0.0f});
  primitive.acoustic_occlusion_trust = finite01(desc.acoustic_occlusion_trust);
  primitive.visual_occlusion_trust = finite01(desc.visual_occlusion_trust);
  primitive.ai_cover_value = finite01(desc.ai_cover_value);
  primitive.traversal_affordance = finite01(desc.traversal_affordance);
  primitive.semantic_lod = finite01(desc.semantic_lod);
  primitive.neural_irradiance = {finite01(desc.neural_irradiance.x),
                                 finite01(desc.neural_irradiance.y),
                                 finite01(desc.neural_irradiance.z)};
  primitive.neural_irradiance_confidence = finite01(desc.neural_irradiance_confidence);
  primitive.signals = normalized(desc.signals);
  primitive.cell_anchors = desc.cell_anchors;
  primitive.surface_patches = desc.surface_patches;
  primitive.contact_zones = desc.contact_zones;
  primitive.residue_channels = desc.residue_channels;

  for (WorldPerceptualCellAnchor &anchor : primitive.cell_anchors) {
    anchor.residency = finite01(anchor.residency);
    anchor.streaming_cost = finite01(anchor.streaming_cost);
    primitive.active_cell_anchor_count +=
        active(anchor.residency) || anchor.cell_hash != 0u ? 1u : 0u;
  }
  for (WorldPerceptualSurfacePatch &patch : primitive.surface_patches) {
    patch.normal = normalizeOr(patch.normal, {0.0f, 1.0f, 0.0f});
    patch.wetness_flow = finite01(patch.wetness_flow);
    patch.exposure_age = finite01(patch.exposure_age);
    patch.thermal_history = finite01(patch.thermal_history);
    patch.chemical_history = finite01(patch.chemical_history);
    patch.material_stability = finite01(patch.material_stability);
    primitive.active_surface_patch_count +=
        active(patch.wetness_flow) || active(patch.exposure_age) || patch.patch_hash != 0u ? 1u
                                                                                           : 0u;
  }
  for (WorldPerceptualContactZone &zone : primitive.contact_zones) {
    zone.normal = normalizeOr(zone.normal, {0.0f, 1.0f, 0.0f});
    zone.contact_field = finite01(zone.contact_field);
    zone.occlusion_trust = finite01(zone.occlusion_trust);
    zone.ai_cover_value = finite01(zone.ai_cover_value);
    zone.traversal_affordance = finite01(zone.traversal_affordance);
    primitive.ai_cover_value = std::max(primitive.ai_cover_value, zone.ai_cover_value);
    primitive.active_contact_zone_count +=
        active(zone.contact_field) || active(zone.occlusion_trust) || zone.zone_hash != 0u ? 1u
                                                                                          : 0u;
  }
  for (WorldPerceptualResidueChannel &channel : primitive.residue_channels) {
    channel.residue = finite01(channel.residue);
    channel.acoustic_occlusion = finite01(channel.acoustic_occlusion);
    channel.ecology_signal = finite01(channel.ecology_signal);
    channel.threat = finite01(channel.threat);
    channel.decision_impact = finite01(channel.decision_impact);
    primitive.active_residue_channel_count +=
        active(channel.residue) || active(channel.acoustic_occlusion) ||
                active(channel.decision_impact) || channel.channel_hash != 0u
            ? 1u
            : 0u;
  }

  const float active_subrecord_score =
      finite01(static_cast<float>(primitive.active_cell_anchor_count +
                                  primitive.active_surface_patch_count +
                                  primitive.active_contact_zone_count +
                                  primitive.active_residue_channel_count) /
               4.0f);
  const float evidence_score = averageSignals(primitive.signals);
  const bool has_identity = primitive.world_owner_hash != 0u && primitive.template_hash != 0u &&
                            primitive.cell_hash != 0u &&
                            primitive.player_readable_cause_hash != 0u;
  if (primitive.sound_surface_class_hash == 0u && has_identity) {
    primitive.sound_surface_class_hash =
        mixString(mix(kWorldPrimitiveSeed, primitive.cell_hash), primitive.object_name);
  }
  primitive.accepted = desc.player_observable && primitive.world_ownership > 0.0f &&
                       primitive.cell_residency > 0.0f && has_identity &&
                       primitive.sound_surface_class_hash != 0u &&
                       evidence_score >= 0.30f &&
                       active_subrecord_score > 0.0f;
  primitive.diagnostic =
      primitive.accepted ? "world perceptual primitive accepted"
                         : "world perceptual primitive needs observable causal evidence";
  primitive.truth_hash = hashPrimitive(desc, primitive);
  return primitive;
}

WorldPerceptualPrimitiveSummary summarizeWorldPerceptualPrimitives(
    const std::vector<WorldPerceptualPrimitive> &primitives) {
  WorldPerceptualPrimitiveSummary summary;
  summary.primitive_count = primitives.size();
  if (primitives.empty()) {
    summary.diagnostic = "no world perceptual primitives";
    return summary;
  }

  std::uint64_t hash = mixString(kWorldPrimitiveSeed, "aster.world-perceptual-summary.v1");
  std::size_t accepted = 0u;
  for (const WorldPerceptualPrimitive &primitive : primitives) {
    hash = mix(hash, primitive.truth_hash);
    summary.active_cell_anchor_count += primitive.active_cell_anchor_count;
    summary.active_surface_patch_count += primitive.active_surface_patch_count;
    summary.active_contact_zone_count += primitive.active_contact_zone_count;
    summary.active_residue_channel_count += primitive.active_residue_channel_count;
    summary.belief_state += primitive.signals.belief_state;
    summary.perceptual_debt += primitive.signals.perceptual_debt;
    summary.material_memory += primitive.signals.material_memory;
    summary.interaction_residue += primitive.signals.interaction_residue;
    summary.contact_field += primitive.signals.contact_field;
    summary.light_history += primitive.signals.light_history;
    summary.acoustic_occlusion += primitive.signals.acoustic_occlusion;
    summary.ecology_pressure += primitive.signals.ecology_pressure;
    summary.threat_gradient += primitive.signals.threat_gradient;
    summary.traversal_pressure += primitive.signals.traversal_pressure;
    summary.semantic_lod += primitive.signals.semantic_lod;
    summary.decision_impact += primitive.signals.decision_impact;
    summary.player_readable_cause += primitive.signals.player_readable_cause;
    accepted += primitive.accepted ? 1u : 0u;
  }
  const float inv_count = 1.0f / static_cast<float>(primitives.size());
  summary.belief_state = finite01(summary.belief_state * inv_count);
  summary.perceptual_debt = finite01(summary.perceptual_debt * inv_count);
  summary.material_memory = finite01(summary.material_memory * inv_count);
  summary.interaction_residue = finite01(summary.interaction_residue * inv_count);
  summary.contact_field = finite01(summary.contact_field * inv_count);
  summary.light_history = finite01(summary.light_history * inv_count);
  summary.acoustic_occlusion = finite01(summary.acoustic_occlusion * inv_count);
  summary.ecology_pressure = finite01(summary.ecology_pressure * inv_count);
  summary.threat_gradient = finite01(summary.threat_gradient * inv_count);
  summary.traversal_pressure = finite01(summary.traversal_pressure * inv_count);
  summary.semantic_lod = finite01(summary.semantic_lod * inv_count);
  summary.decision_impact = finite01(summary.decision_impact * inv_count);
  summary.player_readable_cause = finite01(summary.player_readable_cause * inv_count);
  hash = mix(hash, static_cast<std::uint64_t>(summary.primitive_count));
  hash = mix(hash, static_cast<std::uint64_t>(summary.active_cell_anchor_count));
  hash = mix(hash, static_cast<std::uint64_t>(summary.active_surface_patch_count));
  hash = mix(hash, static_cast<std::uint64_t>(summary.active_contact_zone_count));
  hash = mix(hash, static_cast<std::uint64_t>(summary.active_residue_channel_count));
  hash = mix(hash, summary.decision_impact);
  summary.truth_hash = hash;
  summary.accepted = accepted == primitives.size();
  summary.diagnostic =
      summary.accepted ? "world perceptual primitive summary accepted"
                       : "world perceptual primitive summary has degraded primitives";
  return summary;
}

WorldPerceptualPrimitive makeWorldPerceptualPrimitiveFromRuntime(
    std::string primitive_id, std::string object_name, const PerceptualFrameState &state,
    const PerceptualWorldScheduleReport &schedule, const WorldPerceptionLedgerReport &ledger) {
  WorldPerceptualPrimitiveDesc desc;
  desc.primitive_id = std::move(primitive_id);
  desc.object_name = std::move(object_name);
  desc.world_owner_hash = ledger.region_id;
  desc.template_hash = ledger.ledger_hash;
  desc.cell_hash = ledger.streaming_semantic_lod_hash != 0u ? ledger.streaming_semantic_lod_hash
                                                             : ledger.ledger_hash;
  desc.player_readable_cause_hash = ledger.gameplay_affordance_hash != 0u
                                        ? ledger.gameplay_affordance_hash
                                        : schedule.perceptual_priority_hash;
  desc.sound_surface_class_hash = ledger.audio_visual_cue_budget_hash != 0u
                                      ? ledger.audio_visual_cue_budget_hash
                                      : ledger.ledger_hash;
  desc.delta_seconds = 1.0f / 60.0f;
  desc.exposure_age_seconds = state.exposure_seconds;
  desc.cell_residency = ledger.streaming_semantic_lod_hash != 0u ? 1.0f : 0.55f;
  desc.streaming_cost = 1.0f - schedule.streaming_budget;
  desc.material_stability = 1.0f - schedule.interaction_debt * 0.35f;
  desc.contact_normal_history = {0.0f, 1.0f, 0.0f};
  desc.acoustic_occlusion_trust = state.render_budget.audio;
  desc.visual_occlusion_trust = state.occlusion_trust;
  desc.ai_cover_value = std::max(schedule.threat_signal, state.occlusion_trust * 0.50f);
  desc.traversal_affordance = state.traversal_pressure;
  desc.semantic_lod = state.render_budget.lod_bias;
  desc.signals = {.belief_state = schedule.belief_stability,
                  .perceptual_debt = state.continuity_debt,
                  .material_memory = state.material_memory,
                  .interaction_residue = state.interaction_residue,
                  .contact_field = std::max(state.occlusion_trust, schedule.memory_residue),
                  .light_history = state.lighting_believability,
                  .acoustic_occlusion = 1.0f - state.render_budget.audio,
                  .ecology_pressure = std::max(state.ecology_signal, schedule.material_age),
                  .threat_gradient = schedule.threat_signal,
                  .traversal_pressure = state.traversal_pressure,
                  .semantic_lod = state.render_budget.lod_bias,
                  .decision_impact = schedule.decision_impact_score,
                  .player_readable_cause = state.player_readable_cause};
  desc.cell_anchors.push_back({.id = "runtime.cell",
                               .cell_hash = ledger.ledger_hash,
                               .residency = desc.cell_residency,
                               .streaming_cost = desc.streaming_cost});
  desc.surface_patches.push_back({.id = "runtime.surface",
                                  .patch_hash = ledger.material_memory_hash,
                                  .wetness_flow = state.material_memory,
                                  .exposure_age = finite01(state.exposure_seconds / 47.0f),
                                  .thermal_history = state.lighting_believability,
                                  .chemical_history = schedule.material_age,
                                  .material_stability = desc.material_stability});
  desc.contact_zones.push_back({.id = "runtime.contact",
                                .zone_hash = ledger.contact_history_hash,
                                .contact_field = desc.signals.contact_field,
                                .occlusion_trust = state.occlusion_trust,
                                .ai_cover_value = desc.ai_cover_value,
                                .traversal_affordance = state.traversal_pressure});
  desc.residue_channels.push_back({.id = "runtime.residue",
                                   .channel_hash = schedule.memory_residue_hash,
                                   .residue = state.interaction_residue,
                                   .acoustic_occlusion = desc.signals.acoustic_occlusion,
                                   .ecology_signal = state.ecology_signal,
                                   .threat = schedule.threat_signal,
                                   .decision_impact = schedule.decision_impact_score});
  return evaluateWorldPerceptualPrimitive(desc);
}

void WorldPerceptualField::reset() {
  states_.clear();
}

WorldPerceptualPrimitive
WorldPerceptualField::advance(const WorldPerceptualFieldObservation &observation) {
  const auto found = std::find_if(
      states_.begin(), states_.end(), [&observation](const WorldPerceptualFieldState &state) {
        return state.key == observation.key;
      });
  const float delta_seconds = std::clamp(observation.delta_seconds, 0.0f, 1.0f);
  const float material_half_life = std::max(observation.material_half_life_seconds, 0.001f);
  const WorldPerceptualSignals previous =
      found == states_.end() ? observation.target_signals : found->primitive.signals;
  const float exposure_age =
      (found == states_.end() ? 0.0f : found->exposure_age_seconds) + delta_seconds;

  WorldPerceptualPrimitiveDesc desc;
  desc.primitive_id = observation.primitive_id;
  desc.object_name = observation.object_name;
  desc.world_owner_hash = observation.key.world_owner_hash;
  desc.template_hash = observation.key.template_hash;
  desc.cell_hash = observation.key.cell_hash;
  desc.player_readable_cause_hash = observation.player_readable_cause_hash;
  desc.sound_surface_class_hash = observation.sound_surface_class_hash;
  desc.neural_irradiance_hash = observation.neural_irradiance_hash;
  desc.delta_seconds = delta_seconds;
  desc.wetness_half_life_seconds = material_half_life;
  desc.material_half_life_seconds = material_half_life;
  desc.exposure_age_seconds = exposure_age;
  desc.cell_residency = observation.cell_residency;
  desc.streaming_cost = observation.streaming_cost;
  desc.material_stability = observation.material_stability;
  desc.contact_normal_history = observation.contact_normal;
  desc.acoustic_occlusion_trust = observation.acoustic_occlusion_trust;
  desc.visual_occlusion_trust = observation.visual_occlusion_trust;
  desc.ai_cover_value = observation.ai_cover_value;
  desc.traversal_affordance = observation.traversal_affordance;
  desc.semantic_lod = observation.semantic_lod;
  desc.neural_irradiance = observation.neural_irradiance;
  desc.neural_irradiance_confidence = observation.neural_irradiance_confidence;
  desc.player_observable = observation.player_observable;
  desc.signals =
      decayWorldPerceptualSignals(previous, observation.target_signals, delta_seconds,
                                  material_half_life);
  desc.cell_anchors.push_back({.id = "field.cell",
                               .cell_hash = observation.key.cell_hash,
                               .center = observation.cell_center,
                               .residency = observation.cell_residency,
                               .streaming_cost = observation.streaming_cost});
  desc.surface_patches.push_back({.id = "field.surface",
                                  .patch_hash = observation.key.template_hash,
                                  .normal = observation.contact_normal,
                                  .wetness_flow = desc.signals.material_memory,
                                  .exposure_age = finite01(exposure_age / material_half_life),
                                  .thermal_history = desc.signals.light_history,
                                  .chemical_history = desc.signals.ecology_pressure,
                                  .material_stability = observation.material_stability});
  desc.contact_zones.push_back({.id = "field.contact",
                                .zone_hash = observation.key.cell_hash,
                                .normal = observation.contact_normal,
                                .contact_field = desc.signals.contact_field,
                                .occlusion_trust = observation.visual_occlusion_trust,
                                .ai_cover_value =
                                    std::max(observation.ai_cover_value,
                                             desc.signals.threat_gradient),
                                .traversal_affordance = observation.traversal_affordance});
  desc.residue_channels.push_back({.id = "field.residue",
                                   .channel_hash = observation.player_readable_cause_hash,
                                   .residue = desc.signals.interaction_residue,
                                   .acoustic_occlusion = 1.0f - observation.acoustic_occlusion_trust,
                                   .ecology_signal = desc.signals.ecology_pressure,
                                   .threat = desc.signals.threat_gradient,
                                   .decision_impact = desc.signals.decision_impact});
  WorldPerceptualPrimitive primitive = evaluateWorldPerceptualPrimitive(desc);
  if (found == states_.end()) {
    states_.push_back({.key = observation.key,
                       .exposure_age_seconds = exposure_age,
                       .primitive = primitive});
  } else {
    found->exposure_age_seconds = exposure_age;
    found->primitive = primitive;
  }
  return primitive;
}

const std::vector<WorldPerceptualFieldState> &WorldPerceptualField::states() const noexcept {
  return states_;
}

} // namespace aster
