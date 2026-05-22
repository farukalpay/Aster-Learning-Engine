// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/perceptual_causality_graph.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace aster {
namespace {

constexpr std::uint64_t kCausalitySeed = 0xA57ECA05A1117001ull;

[[nodiscard]] float clamp01(const float value) {
  return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint64_t value) {
  return hashCombine64(hash == 0u ? kCausalitySeed : hash, value);
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::uint32_t value) {
  return mix(hash, static_cast<std::uint64_t>(value));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const std::size_t value) {
  return mix(hash, static_cast<std::uint64_t>(value));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const float value) {
  return mix(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(clamp01(value))));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t hash, const bool value) {
  return mix(hash, static_cast<std::uint64_t>(value ? 1u : 0u));
}

[[nodiscard]] std::uint64_t mixString(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash = mix(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return mix(hash, value.size());
}

[[nodiscard]] bool signalActive(const float value) {
  return clamp01(value) > 0.0001f;
}

[[nodiscard]] std::uint32_t inferChangedMask(const PerceptualCausalityEdgeDesc &edge) {
  std::uint32_t mask = edge.changed_channel_mask;
  if (signalActive(edge.target_signals.material_memory)) {
    mask |= perceptualCausalityChannelBit("material_memory");
  }
  if (signalActive(edge.target_signals.interaction_residue) ||
      signalActive(edge.target_signals.contact_field)) {
    mask |= perceptualCausalityChannelBit("contact_residue");
  }
  if (signalActive(edge.target_signals.light_history)) {
    mask |= perceptualCausalityChannelBit("light_history");
  }
  if (edge.sound_surface_class_hash != 0u || signalActive(edge.target_signals.acoustic_occlusion)) {
    mask |= perceptualCausalityChannelBit("acoustic_surface");
  }
  if (signalActive(edge.traversal_affordance) || signalActive(edge.target_signals.traversal_pressure)) {
    mask |= perceptualCausalityChannelBit("traversal_affordance");
  }
  if (signalActive(edge.ai_cover_value) || signalActive(edge.target_signals.threat_gradient)) {
    mask |= perceptualCausalityChannelBit("threat_cover");
  }
  if (signalActive(edge.semantic_lod) || signalActive(edge.target_signals.semantic_lod)) {
    mask |= perceptualCausalityChannelBit("semantic_lod");
  }
  if (signalActive(edge.streaming_cost)) {
    mask |= perceptualCausalityChannelBit("streaming_cost");
  }
  if (edge.player_readable_cause_hash != 0u || signalActive(edge.target_signals.player_readable_cause)) {
    mask |= perceptualCausalityChannelBit("player_readable_cause");
  }
  if (edge.neural_irradiance_hash != 0u || signalActive(edge.neural_irradiance_confidence)) {
    mask |= perceptualCausalityChannelBit("neural_irradiance");
  }
  return mask;
}

[[nodiscard]] std::uint32_t inferDecisionMask(const PerceptualCausalityEdgeDesc &edge,
                                              const std::uint32_t changed_mask) {
  std::uint32_t mask = edge.decision_channel_mask;
  if (edge.target_signals.decision_impact > 0.0f || edge.target_signals.player_readable_cause > 0.0f) {
    mask |= changed_mask & (perceptualCausalityChannelBit("player_readable_cause") |
                            perceptualCausalityChannelBit("threat_cover") |
                            perceptualCausalityChannelBit("traversal_affordance") |
                            perceptualCausalityChannelBit("acoustic_surface") |
                            perceptualCausalityChannelBit("material_memory") |
                            perceptualCausalityChannelBit("light_history"));
  }
  return mask;
}

[[nodiscard]] WorldPerceptualPrimitive
evaluateCausalPrimitive(const PerceptualCausalityEdgeDesc &edge,
                        const WorldPerceptualSignals &previous,
                        const float exposure_age_seconds,
                        const float material_half_life_seconds) {
  WorldPerceptualPrimitiveDesc desc;
  desc.primitive_id = edge.primitive_id;
  desc.object_name = edge.object_name;
  desc.world_owner_hash = edge.key.world_owner_hash;
  desc.template_hash = edge.key.template_hash;
  desc.cell_hash = edge.key.cell_hash;
  desc.player_readable_cause_hash = edge.player_readable_cause_hash;
  desc.sound_surface_class_hash = edge.sound_surface_class_hash;
  desc.neural_irradiance_hash = edge.neural_irradiance_hash;
  desc.delta_seconds = std::clamp(edge.delta_seconds, 0.0f, 1.0f);
  desc.wetness_half_life_seconds = material_half_life_seconds;
  desc.material_half_life_seconds = material_half_life_seconds;
  desc.exposure_age_seconds = exposure_age_seconds;
  desc.cell_residency = edge.cell_residency;
  desc.streaming_cost = edge.streaming_cost;
  desc.material_stability = edge.material_stability;
  desc.contact_normal_history = edge.contact_normal;
  desc.acoustic_occlusion_trust = edge.acoustic_occlusion_trust;
  desc.visual_occlusion_trust = edge.visual_occlusion_trust;
  desc.ai_cover_value = edge.ai_cover_value;
  desc.traversal_affordance = edge.traversal_affordance;
  desc.semantic_lod = edge.semantic_lod;
  desc.neural_irradiance = edge.neural_irradiance;
  desc.neural_irradiance_confidence = edge.neural_irradiance_confidence;
  desc.changed_channel_mask = inferChangedMask(edge);
  desc.decision_channel_mask = inferDecisionMask(edge, desc.changed_channel_mask);
  desc.player_observable = edge.player_observable;
  desc.signals = decayWorldPerceptualSignals(previous, edge.target_signals, desc.delta_seconds,
                                             material_half_life_seconds);
  desc.cell_anchors.push_back({.id = "causality.cell",
                               .cell_hash = edge.key.cell_hash,
                               .center = edge.cell_center,
                               .residency = edge.cell_residency,
                               .streaming_cost = edge.streaming_cost});
  desc.surface_patches.push_back({.id = "causality.surface",
                                  .patch_hash = edge.key.template_hash,
                                  .normal = edge.contact_normal,
                                  .wetness_flow = desc.signals.material_memory,
                                  .exposure_age =
                                      clamp01(exposure_age_seconds / material_half_life_seconds),
                                  .thermal_history = desc.signals.light_history,
                                  .chemical_history = desc.signals.ecology_pressure,
                                  .material_stability = edge.material_stability});
  desc.contact_zones.push_back({.id = "causality.contact",
                                .zone_hash = edge.key.cell_hash,
                                .normal = edge.contact_normal,
                                .contact_field = desc.signals.contact_field,
                                .occlusion_trust = edge.visual_occlusion_trust,
                                .ai_cover_value = std::max(edge.ai_cover_value,
                                                           desc.signals.threat_gradient),
                                .traversal_affordance = edge.traversal_affordance});
  desc.residue_channels.push_back({.id = "causality.residue",
                                   .channel_hash = edge.player_readable_cause_hash,
                                   .residue = desc.signals.interaction_residue,
                                   .acoustic_occlusion = desc.signals.acoustic_occlusion,
                                   .ecology_signal = desc.signals.ecology_pressure,
                                   .threat = desc.signals.threat_gradient,
                                   .decision_impact = desc.signals.decision_impact});
  return evaluateWorldPerceptualPrimitive(desc);
}

[[nodiscard]] std::uint64_t hashReport(const PerceptualCausalityGraphOptions &options,
                                       const PerceptualCausalityGraphReport &report,
                                       const std::vector<WorldPerceptualPrimitive> &primitives) {
  std::uint64_t hash = mixString(kCausalitySeed, "aster.perceptual-causality-graph.v1");
  hash = mixString(hash, options.id);
  hash = mix(hash, options.region_id);
  hash = mix(hash, options.required_changed_channel_mask);
  hash = mix(hash, options.required_decision_channel_mask);
  hash = mix(hash, options.minimum_decision_impact);
  hash = mix(hash, report.source_world_transition_hash);
  hash = mix(hash, report.primitive_count);
  hash = mix(hash, report.changed_channel_mask);
  hash = mix(hash, report.decision_channel_mask);
  hash = mix(hash, report.decision_impact_score);
  hash = mix(hash, report.accepted);
  for (const WorldPerceptualPrimitive &primitive : primitives) {
    hash = mix(hash, primitive.truth_hash);
    hash = mixString(hash, primitive.primitive_id);
    hash = mix(hash, primitive.signals.decision_impact);
    hash = mix(hash, primitive.signals.player_readable_cause);
  }
  return hash;
}

} // namespace

std::uint32_t perceptualCausalityChannelBit(const std::string_view channel) {
  if (channel == "material_memory") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::MaterialMemory);
  }
  if (channel == "contact_residue" || channel == "contact_history") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::ContactResidue);
  }
  if (channel == "light_history" || channel == "lighting_exposure") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::LightHistory);
  }
  if (channel == "acoustic_surface" || channel == "audio_visual_cue_budget") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::AcousticSurface);
  }
  if (channel == "traversal_affordance" || channel == "traversal_pressure") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::TraversalAffordance);
  }
  if (channel == "threat_cover" || channel == "ai_attention") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::ThreatCover);
  }
  if (channel == "semantic_lod" || channel == "streaming_semantic_lod") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::SemanticLod);
  }
  if (channel == "streaming_cost" || channel == "streaming_residency") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::StreamingCost);
  }
  if (channel == "player_readable_cause" || channel == "gameplay_affordance") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::PlayerReadableCause);
  }
  if (channel == "neural_irradiance") {
    return static_cast<std::uint32_t>(PerceptualCausalityChannel::NeuralIrradiance);
  }
  return 0u;
}

std::string_view perceptualCausalityChannelName(const PerceptualCausalityChannel channel) {
  switch (channel) {
  case PerceptualCausalityChannel::MaterialMemory:
    return "material_memory";
  case PerceptualCausalityChannel::ContactResidue:
    return "contact_residue";
  case PerceptualCausalityChannel::LightHistory:
    return "light_history";
  case PerceptualCausalityChannel::AcousticSurface:
    return "acoustic_surface";
  case PerceptualCausalityChannel::TraversalAffordance:
    return "traversal_affordance";
  case PerceptualCausalityChannel::ThreatCover:
    return "threat_cover";
  case PerceptualCausalityChannel::SemanticLod:
    return "semantic_lod";
  case PerceptualCausalityChannel::StreamingCost:
    return "streaming_cost";
  case PerceptualCausalityChannel::PlayerReadableCause:
    return "player_readable_cause";
  case PerceptualCausalityChannel::NeuralIrradiance:
    return "neural_irradiance";
  }
  return "unknown";
}

std::uint32_t perceptualCausalityAllChannelMask() {
  return perceptualCausalityChannelBit("material_memory") |
         perceptualCausalityChannelBit("contact_residue") |
         perceptualCausalityChannelBit("light_history") |
         perceptualCausalityChannelBit("acoustic_surface") |
         perceptualCausalityChannelBit("traversal_affordance") |
         perceptualCausalityChannelBit("threat_cover") |
         perceptualCausalityChannelBit("semantic_lod") |
         perceptualCausalityChannelBit("streaming_cost") |
         perceptualCausalityChannelBit("player_readable_cause") |
         perceptualCausalityChannelBit("neural_irradiance");
}

PerceptualCausalityGraph::PerceptualCausalityGraph(PerceptualCausalityGraphOptions options)
    : options_(std::move(options)) {}

void PerceptualCausalityGraph::setOptions(PerceptualCausalityGraphOptions options) {
  options.minimum_decision_impact = clamp01(options.minimum_decision_impact);
  options.default_material_half_life_seconds =
      std::max(options.default_material_half_life_seconds, 0.001f);
  if (options_.id != options.id || options_.region_id != options.region_id) {
    reset();
  }
  options_ = std::move(options);
}

const PerceptualCausalityGraphOptions &PerceptualCausalityGraph::options() const noexcept {
  return options_;
}

void PerceptualCausalityGraph::reset() {
  states_.clear();
  primitives_.clear();
  report_ = {};
}

PerceptualCausalityGraphResult
PerceptualCausalityGraph::advance(const std::vector<PerceptualCausalityEdgeDesc> &edges) {
  primitives_.clear();
  primitives_.reserve(edges.size());
  PerceptualCausalityGraphReport report;
  report.primitive_count = 0u;
  std::size_t accepted_count = 0u;

  for (const PerceptualCausalityEdgeDesc &edge : edges) {
    if (!edge.key.valid() || edge.primitive_id.empty()) {
      continue;
    }
    const float delta_seconds = std::clamp(edge.delta_seconds, 0.0f, 1.0f);
    const float material_half_life =
        std::max(edge.material_half_life_seconds > 0.0f ? edge.material_half_life_seconds
                                                        : options_.default_material_half_life_seconds,
                 0.001f);
    const auto found = std::find_if(
        states_.begin(), states_.end(), [&edge](const NodeState &state) {
          return state.key == edge.key && state.primitive_id == edge.primitive_id;
        });
    const WorldPerceptualSignals previous =
        found == states_.end() ? edge.target_signals : found->primitive.signals;
    const float exposure_age =
        (found == states_.end() ? 0.0f : found->exposure_age_seconds) + delta_seconds;
    WorldPerceptualPrimitive primitive =
        evaluateCausalPrimitive(edge, previous, exposure_age, material_half_life);
    primitives_.push_back(primitive);
    if (found == states_.end()) {
      states_.push_back({.key = edge.key,
                         .primitive_id = edge.primitive_id,
                         .exposure_age_seconds = exposure_age,
                         .primitive = std::move(primitive)});
    } else {
      found->exposure_age_seconds = exposure_age;
      found->primitive = std::move(primitive);
    }

    const std::uint32_t changed_mask = inferChangedMask(edge);
    const std::uint32_t decision_mask = inferDecisionMask(edge, changed_mask);
    report.changed_channel_mask |= changed_mask;
    report.decision_channel_mask |= decision_mask;
    report.source_world_transition_hash =
        edge.source_world_transition_hash != 0u ? edge.source_world_transition_hash
                                                : report.source_world_transition_hash;
    report.decision_impact_score =
        std::max(report.decision_impact_score,
                 std::max(primitives_.back().signals.decision_impact,
                          primitives_.back().signals.player_readable_cause * 0.72f));
    accepted_count += primitives_.back().accepted ? 1u : 0u;
  }

  report.primitive_count = primitives_.size();
  const bool changed_ok = (options_.required_changed_channel_mask == 0u) ||
                          ((report.changed_channel_mask & options_.required_changed_channel_mask) ==
                           options_.required_changed_channel_mask);
  const bool decision_ok = (options_.required_decision_channel_mask == 0u) ||
                           ((report.decision_channel_mask &
                             options_.required_decision_channel_mask) ==
                            options_.required_decision_channel_mask);
  report.decision_impact_score = clamp01(report.decision_impact_score);
  report.accepted = report.primitive_count > 0u && accepted_count == report.primitive_count &&
                    changed_ok && decision_ok &&
                    report.decision_impact_score + 0.0001f >=
                        options_.minimum_decision_impact;
  report.diagnostic =
      report.accepted ? "perceptual causality graph accepted"
                      : "perceptual causality graph missing required causal decision evidence";
  report.graph_hash = hashReport(options_, report, primitives_);
  report_ = report;
  return {.report = report_, .primitives = primitives_};
}

const PerceptualCausalityGraphReport &PerceptualCausalityGraph::lastReport() const noexcept {
  return report_;
}

const std::vector<WorldPerceptualPrimitive> &
PerceptualCausalityGraph::lastPrimitives() const noexcept {
  return primitives_;
}

} // namespace aster
