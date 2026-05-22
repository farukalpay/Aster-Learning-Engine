// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/world_perceptual_primitive.hpp"
#include "aster/math/vec.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class PerceptualCausalityChannel : std::uint32_t {
  MaterialMemory = 1u << 0u,
  ContactResidue = 1u << 1u,
  LightHistory = 1u << 2u,
  AcousticSurface = 1u << 3u,
  TraversalAffordance = 1u << 4u,
  ThreatCover = 1u << 5u,
  SemanticLod = 1u << 6u,
  StreamingCost = 1u << 7u,
  PlayerReadableCause = 1u << 8u,
  NeuralIrradiance = 1u << 9u,
};

[[nodiscard]] std::uint32_t perceptualCausalityChannelBit(std::string_view channel);
[[nodiscard]] std::string_view perceptualCausalityChannelName(PerceptualCausalityChannel channel);
[[nodiscard]] std::uint32_t perceptualCausalityAllChannelMask();

struct PerceptualCausalityGraphOptions {
  std::uint64_t region_id = 0u;
  std::string id = "perceptual_causality_graph";
  std::uint32_t required_changed_channel_mask = 0u;
  std::uint32_t required_decision_channel_mask = 0u;
  float minimum_decision_impact = 0.50f;
  float default_material_half_life_seconds = 12.0f;
};

struct PerceptualCausalityEdgeDesc {
  std::string primitive_id;
  std::string object_name;
  std::uint64_t source_world_transition_hash = 0u;
  WorldPerceptualFieldKey key;
  std::uint64_t player_readable_cause_hash = 0u;
  std::uint64_t sound_surface_class_hash = 0u;
  std::uint64_t neural_irradiance_hash = 0u;
  Vec3 cell_center{};
  Vec3 contact_normal{0.0f, 1.0f, 0.0f};
  float delta_seconds = 1.0f / 60.0f;
  float material_half_life_seconds = 0.0f;
  float cell_residency = 0.0f;
  float streaming_cost = 0.0f;
  float material_stability = 1.0f;
  float acoustic_occlusion_trust = 0.0f;
  float visual_occlusion_trust = 0.0f;
  float ai_cover_value = 0.0f;
  float traversal_affordance = 0.0f;
  float semantic_lod = 0.0f;
  Vec3 neural_irradiance{};
  float neural_irradiance_confidence = 0.0f;
  bool player_observable = true;
  std::uint32_t changed_channel_mask = 0u;
  std::uint32_t decision_channel_mask = 0u;
  WorldPerceptualSignals target_signals;
};

struct PerceptualCausalityGraphReport {
  bool accepted = false;
  std::uint64_t graph_hash = 0u;
  std::uint64_t source_world_transition_hash = 0u;
  std::size_t primitive_count = 0u;
  std::uint32_t changed_channel_mask = 0u;
  std::uint32_t decision_channel_mask = 0u;
  float decision_impact_score = 0.0f;
  std::string diagnostic = "perceptual causality graph has not advanced";
};

struct PerceptualCausalityGraphResult {
  PerceptualCausalityGraphReport report;
  std::vector<WorldPerceptualPrimitive> primitives;
};

class PerceptualCausalityGraph {
public:
  PerceptualCausalityGraph() = default;
  explicit PerceptualCausalityGraph(PerceptualCausalityGraphOptions options);

  void setOptions(PerceptualCausalityGraphOptions options);
  [[nodiscard]] const PerceptualCausalityGraphOptions &options() const noexcept;
  void reset();
  [[nodiscard]] PerceptualCausalityGraphResult
  advance(const std::vector<PerceptualCausalityEdgeDesc> &edges);
  [[nodiscard]] const PerceptualCausalityGraphReport &lastReport() const noexcept;
  [[nodiscard]] const std::vector<WorldPerceptualPrimitive> &lastPrimitives() const noexcept;

private:
  struct NodeState {
    WorldPerceptualFieldKey key;
    std::string primitive_id;
    float exposure_age_seconds = 0.0f;
    WorldPerceptualPrimitive primitive;
  };

  PerceptualCausalityGraphOptions options_{};
  PerceptualCausalityGraphReport report_{};
  std::vector<NodeState> states_;
  std::vector<WorldPerceptualPrimitive> primitives_;
};

} // namespace aster
