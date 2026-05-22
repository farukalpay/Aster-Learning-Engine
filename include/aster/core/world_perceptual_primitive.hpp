// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/perceptual_world_runtime.hpp"
#include "aster/math/vec.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster {

struct WorldPerceptualSignals {
  float belief_state = 0.0f;
  float perceptual_debt = 1.0f;
  float material_memory = 0.0f;
  float interaction_residue = 0.0f;
  float contact_field = 0.0f;
  float light_history = 0.0f;
  float acoustic_occlusion = 0.0f;
  float ecology_pressure = 0.0f;
  float threat_gradient = 0.0f;
  float traversal_pressure = 0.0f;
  float semantic_lod = 0.0f;
  float decision_impact = 0.0f;
  float player_readable_cause = 0.0f;
};

struct WorldPerceptualCellAnchor {
  std::string id;
  std::uint64_t cell_hash = 0u;
  Vec3 center{};
  float residency = 0.0f;
  float streaming_cost = 0.0f;
};

struct WorldPerceptualSurfacePatch {
  std::string id;
  std::uint64_t patch_hash = 0u;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float wetness_flow = 0.0f;
  float exposure_age = 0.0f;
  float thermal_history = 0.0f;
  float chemical_history = 0.0f;
  float material_stability = 1.0f;
};

struct WorldPerceptualContactZone {
  std::string id;
  std::uint64_t zone_hash = 0u;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float contact_field = 0.0f;
  float occlusion_trust = 0.0f;
  float ai_cover_value = 0.0f;
  float traversal_affordance = 0.0f;
};

struct WorldPerceptualResidueChannel {
  std::string id;
  std::uint64_t channel_hash = 0u;
  float residue = 0.0f;
  float acoustic_occlusion = 0.0f;
  float ecology_signal = 0.0f;
  float threat = 0.0f;
  float decision_impact = 0.0f;
};

struct WorldPerceptualPrimitiveDesc {
  std::string primitive_id;
  std::string object_name;
  std::uint64_t world_owner_hash = 0u;
  std::uint64_t player_readable_cause_hash = 0u;
  float delta_seconds = 1.0f / 60.0f;
  float wetness_half_life_seconds = 12.0f;
  float exposure_age_seconds = 0.0f;
  float world_ownership = 1.0f;
  float cell_residency = 0.0f;
  float streaming_cost = 0.0f;
  float material_stability = 1.0f;
  bool player_observable = true;
  WorldPerceptualSignals signals;
  std::vector<WorldPerceptualCellAnchor> cell_anchors;
  std::vector<WorldPerceptualSurfacePatch> surface_patches;
  std::vector<WorldPerceptualContactZone> contact_zones;
  std::vector<WorldPerceptualResidueChannel> residue_channels;
};

struct WorldPerceptualPrimitive {
  std::string primitive_id;
  std::string object_name;
  std::uint64_t truth_hash = 0u;
  std::uint64_t world_owner_hash = 0u;
  std::uint64_t player_readable_cause_hash = 0u;
  WorldPerceptualSignals signals;
  float cell_residency = 0.0f;
  float world_ownership = 0.0f;
  float wetness_half_life_seconds = 0.0f;
  float exposure_age_seconds = 0.0f;
  float streaming_cost = 0.0f;
  float material_stability = 1.0f;
  std::size_t active_cell_anchor_count = 0u;
  std::size_t active_surface_patch_count = 0u;
  std::size_t active_contact_zone_count = 0u;
  std::size_t active_residue_channel_count = 0u;
  bool accepted = false;
  std::string diagnostic;
  std::vector<WorldPerceptualCellAnchor> cell_anchors;
  std::vector<WorldPerceptualSurfacePatch> surface_patches;
  std::vector<WorldPerceptualContactZone> contact_zones;
  std::vector<WorldPerceptualResidueChannel> residue_channels;
};

struct WorldPerceptualPrimitiveSummary {
  std::size_t primitive_count = 0u;
  std::size_t active_cell_anchor_count = 0u;
  std::size_t active_surface_patch_count = 0u;
  std::size_t active_contact_zone_count = 0u;
  std::size_t active_residue_channel_count = 0u;
  std::uint64_t truth_hash = 0u;
  float belief_state = 0.0f;
  float perceptual_debt = 1.0f;
  float material_memory = 0.0f;
  float interaction_residue = 0.0f;
  float contact_field = 0.0f;
  float light_history = 0.0f;
  float acoustic_occlusion = 0.0f;
  float ecology_pressure = 0.0f;
  float threat_gradient = 0.0f;
  float traversal_pressure = 0.0f;
  float semantic_lod = 0.0f;
  float decision_impact = 0.0f;
  float player_readable_cause = 0.0f;
  bool accepted = false;
  std::string diagnostic;
};

[[nodiscard]] WorldPerceptualSignals
decayWorldPerceptualSignals(const WorldPerceptualSignals &previous,
                            const WorldPerceptualSignals &target,
                            float delta_seconds,
                            float wetness_half_life_seconds);

[[nodiscard]] WorldPerceptualPrimitive
evaluateWorldPerceptualPrimitive(const WorldPerceptualPrimitiveDesc &desc);

[[nodiscard]] WorldPerceptualPrimitiveSummary
summarizeWorldPerceptualPrimitives(const std::vector<WorldPerceptualPrimitive> &primitives);

[[nodiscard]] WorldPerceptualPrimitive
makeWorldPerceptualPrimitiveFromRuntime(std::string primitive_id,
                                        std::string object_name,
                                        const PerceptualFrameState &state,
                                        const PerceptualWorldScheduleReport &schedule,
                                        const WorldPerceptionLedgerReport &ledger);

} // namespace aster
