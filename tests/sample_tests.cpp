// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

void setEnvFlag(const char *name, const bool enabled) {
#if defined(_WIN32)
  (void)_putenv_s(name, enabled ? "1" : "");
#else
  if (enabled) {
    (void)setenv(name, "1", 1);
  } else {
    (void)unsetenv(name);
  }
#endif
}

void testLumenSceneCoherenceReport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::SceneCoherenceReport &report = run.sceneCoherenceReport();
  assert(!report.contributions.empty());
  assert(report.energy >= 0.0f);
  assert(findContribution(report, aster::SceneCoherenceTerm::FluidContainment) != nullptr);
  assert(findContribution(report, aster::SceneCoherenceTerm::RouteCollision) != nullptr);
  const aster::SceneTraceValidationReport &trace_report = run.sceneTraceReport();
  assert(trace_report.trace_length > 0u);
  assert(!trace_report.rules.empty());
  assert(trace_report.valid);
}

void testLumenWorldForensicsContract() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::LumenCaveWorldGateReport &gate = run.caveWorldGateReport();
  assert(run.caveWorldGateAccepted());
  assert(gate.verdict == aster::LumenWorldGateVerdict::Accepted);
  assert(gate.navigation_valid);
  assert(gate.resource_valid);
  assert(gate.encounter_valid);
  assert(gate.perceptual_valid);
  assert(gate.perception_ledger_valid);
  assert(gate.perception_ledger_hash != 0u);
  assert(gate.perception_ledger_cell_count >= 1u);
  assert(gate.perception_ledger_missing_channel_mask == 0u);
  assert(gate.probe_trace_hash != 0u);
  assert(gate.region_id != 0u);

  const std::uint64_t gate_transition = run.worldForensics().world_transition_hash;
  run.update(1.0f / 60.0f, {0.25f, 0.70f}, true, false);
  const aster::LumenWorldForensics &world = run.worldForensics();
  assert(world.epoch == 1u);
  assert(world.world_transition_hash != 0u);
  assert(world.world_transition_hash != gate_transition);
  assert(world.actor_state_delta_hash != 0u);
  assert(world.sensory_event_hash != 0u);
  assert(world.visibility_set_hash != 0u);
  assert(world.streaming_region_id == gate.region_id);
  assert(world.perception_ledger.accepted);
  assert(world.perception_ledger.ledger_hash != 0u);
  assert(world.perceptual_state.perceptual_state_hash != 0u);
  assert(world.perceptual_state.semantic_budget_hash != 0u);
  assert(world.perceptual_state.material_memory > 0.0f);
  assert(world.perceptual_state.occlusion_trust > 0.0f);
  assert(world.perceptual_schedule.scheduler_hash != 0u);
  assert(world.perceptual_schedule.memory_residue_hash != 0u);
  assert(world.perceptual_schedule.threat_signal_hash != 0u);
  assert(world.perceptual_schedule.material_age_hash != 0u);
  assert(world.perceptual_schedule.streaming_budget_hash != 0u);
  assert(world.perceptual_schedule.streaming_budget > 0.0f);
  assert(world.perceptual_schedule.decision_impact_score > 0.0f);
  assert(world.belief_report.belief_contract_hash != 0u);
  assert(world.belief_report.readability_audit_hash != 0u);
  assert(world.belief_report.score >= 0.70f);
  assert(!world.perception_object_traces.empty());

  run.noteRenderExtraction(0xA57E1001u, 0xA57E2002u, 12.5f);
  assert(run.worldForensics().render_extraction_ready);
  assert(run.worldForensics().render_extraction_hash == 0xA57E1001u);
  assert(run.worldForensics().frame_submission_hash == 0xA57E2002u);
  assert(run.worldForensics().perceptual_schedule.frame_cost_ms >= 12.0f);
}

void testLumenCoalMiningReactionContinuity() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  aster::Vec3 ore_position{};
  bool found_ore = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Coal ore vein node") {
      ore_position = object.transform.position;
      found_ore = true;
      break;
    }
  }
  assert(found_ore);
  assert(run.takeChestItem("pickaxe"));

  const aster::Vec3 player_position = ore_position + aster::Vec3{0.0f, 0.05f, 1.45f};
  run.relocatePlayer(player_position, aster::radians(180.0f));
  const aster::Vec3 focus_origin = player_position + aster::Vec3{0.0f, 0.42f, 0.0f};
  run.updateInteractionFocus(focus_origin, aster::normalize(ore_position - focus_origin),
                             1.0f / 60.0f);
  const aster::FocusPromptModel prompt = run.focusPromptModel();
  assert(prompt.visible);
  assert(prompt.action == "Mine");
  assert(prompt.subject == "Coal Ore");

  const std::uint64_t before_hash =
      run.worldForensics().coal_mining_reaction.reaction_package_hash;
  const std::uint64_t before_perceptual_hash =
      run.worldForensics().perceptual_state.perceptual_state_hash;
  const std::uint64_t before_scheduler_hash =
      run.worldForensics().perceptual_schedule.scheduler_hash;
  run.interactFocused();
  run.update(1.0f / 60.0f, {}, false, false);
  const aster::LumenReactionPackageReport &reaction =
      run.worldForensics().coal_mining_reaction;
  assert(reaction.accepted);
  assert(reaction.reaction_package_hash != 0u);
  assert(reaction.reaction_package_hash != before_hash);
  assert(reaction.missing_channel_mask == 0u);
  assert(reaction.material_memory_hash != 0u);
  assert(reaction.contact_history_hash != 0u);
  assert(reaction.event_residue_hash != 0u);
  assert(reaction.wear_continuity_hash != 0u);
  assert(reaction.audio_visual_cue_budget_hash != 0u);
  assert(reaction.ai_attention_hash != 0u);
  assert(reaction.resource_state_hash != 0u);
  assert(reaction.readability_audit_hash != 0u);
  assert(run.worldForensics().perception_ledger.accepted);
  assert(run.worldForensics().perception_ledger.wear_continuity_hash != 0u);
  assert(run.worldForensics().perceptual_state.perceptual_state_hash != 0u);
  assert(run.worldForensics().perceptual_state.perceptual_state_hash != before_perceptual_hash);
  assert(run.worldForensics().perceptual_state.interaction_residue > 0.0f);
  assert(run.worldForensics().perceptual_state.player_readable_cause > 0.0f);
  assert(run.worldForensics().perceptual_schedule.scheduler_hash != 0u);
  assert(run.worldForensics().perceptual_schedule.scheduler_hash != before_scheduler_hash);
  assert(run.worldForensics().perceptual_schedule.memory_residue > 0.0f);
  assert(run.worldForensics().perceptual_schedule.material_age > 0.0f);
  assert(run.worldForensics().perceptual_schedule.interaction_debt > 0.0f);
  assert(run.worldForensics().perceptual_schedule.decision_impact_score > 0.0f);
}

void testLumenWorldRenderablePerceptualCoverage() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  run.update(1.0f / 60.0f, {}, false, false);

  std::size_t expected = 0u;
  std::size_t observed = 0u;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.perceptual_truth_mode == aster::RenderPerceptualTruthMode::Compatibility) {
      continue;
    }
    ++expected;
    if (object.perceptual_primitive.truth_hash != 0u && object.perceptual_primitive.accepted) {
      ++observed;
    }
  }

  const aster::LumenWorldForensics &world = run.worldForensics();
  assert(expected > 0u);
  assert(observed == expected);
  assert(world.perceptual_primitive_summary.accepted);
  assert(world.perceptual_primitive_summary.primitive_count >= observed);
  assert(std::any_of(world.perceptual_primitives.begin(), world.perceptual_primitives.end(),
                     [](const aster::WorldPerceptualPrimitive &primitive) {
                       return primitive.primitive_id.find("lumen.scene.object.") == 0u &&
                              primitive.accepted;
                     }));
}

struct LumenMineOreTorchReplayHashes {
  std::uint64_t world_hash = 0u;
  std::uint64_t primitive_truth_hash = 0u;
  std::uint64_t belief_hash = 0u;
  std::uint64_t render_extraction_hash = 0u;
  std::uint64_t ai_visibility_hash = 0u;
  std::uint64_t streaming_budget_hash = 0u;
  std::uint64_t audit_hash = 0u;
};

struct LumenPerceptualCausalityReplayProof {
  std::uint64_t world_hash = 0u;
  std::uint64_t graph_hash = 0u;
  std::uint64_t primitive_truth_hash = 0u;
  std::uint64_t render_extraction_hash = 0u;
  std::uint64_t audit_hash = 0u;
  std::uint32_t changed_channel_mask = 0u;
  std::uint32_t decision_channel_mask = 0u;
  float decision_impact_score = 0.0f;
};

LumenMineOreTorchReplayHashes runMineOreTorchReplay() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  aster::Vec3 ore_position{};
  bool found_ore = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Coal ore vein node") {
      ore_position = object.transform.position;
      found_ore = true;
      break;
    }
  }
  assert(found_ore);
  run.relocatePlayer(run.supplyCratePosition(), 0.0f);
  assert(run.takeSupplyTorch());
  assert(run.takeChestItem("pickaxe"));
  const aster::Vec3 player_position = ore_position + aster::Vec3{0.0f, 0.05f, 1.45f};
  run.relocatePlayer(player_position, aster::radians(180.0f));
  const aster::Vec3 focus_origin = player_position + aster::Vec3{0.0f, 0.42f, 0.0f};
  run.updateInteractionFocus(focus_origin, aster::normalize(ore_position - focus_origin),
                             1.0f / 60.0f);
  run.interactFocused();
  run.update(1.0f / 60.0f, {}, false, false);
  run.noteRenderExtraction(0xA57E7101u, 0xA57E7102u, 11.75f);

  const aster::LumenWorldForensics &world = run.worldForensics();
  assert(world.render_extraction_ready);
  assert(world.perceptual_primitive_summary.truth_hash != 0u);
  assert(!world.perceptual_primitives.empty());
  assert(world.world_truth_audit_hash != 0u);
  assert(std::any_of(run.scene().objects().begin(), run.scene().objects().end(),
                     [](const aster::RenderObject &object) {
                       return object.name == "Coal ore vein node" &&
                              object.perceptual_primitive.truth_hash != 0u &&
                              object.perceptual_primitive.signals.interaction_residue > 0.0f;
                     }));
  return {.world_hash = world.world_hash,
          .primitive_truth_hash = world.perceptual_primitive_summary.truth_hash,
          .belief_hash = world.belief_report.belief_contract_hash,
          .render_extraction_hash = world.render_extraction_hash,
          .ai_visibility_hash = world.coal_mining_reaction.ai_attention_hash,
          .streaming_budget_hash = world.perceptual_schedule.streaming_budget_hash,
          .audit_hash = world.world_truth_audit_hash};
}

LumenPerceptualCausalityReplayProof runPerceptualCausalityReplay240() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  aster::Vec3 ore_position{};
  bool found_ore = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Coal ore vein node") {
      ore_position = object.transform.position;
      found_ore = true;
      break;
    }
  }
  assert(found_ore);

  run.relocatePlayer(run.supplyCratePosition(), 0.0f);
  assert(run.takeChestItem("torch"));
  assert(run.takeChestItem("pickaxe"));
  run.selectHotbarSlot(0);
  const aster::Vec3 mine_position = ore_position + aster::Vec3{0.0f, 0.05f, 1.45f};
  run.relocatePlayer(mine_position, aster::radians(180.0f));
  for (int frame = 0; frame < 40; ++frame) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  assert(run.equippedLight().has_value());

  run.relocatePlayer(mine_position, aster::radians(180.0f));
  const aster::Vec3 focus_origin = mine_position + aster::Vec3{0.0f, 0.42f, 0.0f};
  bool mined = false;
  for (std::size_t slot = 0; slot < 6u && !mined; ++slot) {
    run.selectHotbarSlot(slot);
    run.updateInteractionFocus(focus_origin, aster::normalize(ore_position - focus_origin),
                               1.0f / 60.0f);
    run.interactFocused();
    mined = run.worldForensics().coal_mining_reaction.ai_attention_hash != 0u;
  }
  assert(mined);
  run.update(1.0f / 60.0f, {}, false, false);

  run.relocatePlayer(ore_position + aster::Vec3{0.0f, 0.05f, 3.20f}, aster::radians(180.0f));
  for (int frame = 0; frame < 80; ++frame) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  run.relocatePlayer(mine_position, aster::radians(180.0f));
  for (int frame = 0; frame < 119; ++frame) {
    run.update(1.0f / 60.0f, {}, false, false);
  }

  run.noteRenderExtraction(0xA57E7201u, 0xA57E7202u, 12.25f);
  const aster::LumenWorldForensics &world = run.worldForensics();
  const std::uint32_t required_changed =
      aster::perceptualCausalityChannelBit("material_memory") |
      aster::perceptualCausalityChannelBit("contact_residue") |
      aster::perceptualCausalityChannelBit("light_history") |
      aster::perceptualCausalityChannelBit("acoustic_surface") |
      aster::perceptualCausalityChannelBit("traversal_affordance") |
      aster::perceptualCausalityChannelBit("threat_cover") |
      aster::perceptualCausalityChannelBit("player_readable_cause");
  const std::uint32_t required_decision =
      aster::perceptualCausalityChannelBit("material_memory") |
      aster::perceptualCausalityChannelBit("light_history") |
      aster::perceptualCausalityChannelBit("acoustic_surface") |
      aster::perceptualCausalityChannelBit("threat_cover") |
      aster::perceptualCausalityChannelBit("player_readable_cause");
  assert(world.render_extraction_ready);
  assert(world.perceptual_causality_graph.accepted);
  assert(world.perceptual_causality_graph.graph_hash != 0u);
  assert((world.perceptual_causality_graph.changed_channel_mask & required_changed) ==
         required_changed);
  assert((world.perceptual_causality_graph.decision_channel_mask & required_decision) ==
         required_decision);
  assert(world.perceptual_causality_graph.decision_impact_score >= 0.50f);
  assert(world.perceptual_primitive_summary.truth_hash != 0u);
  assert(world.perceptual_primitive_summary.material_memory > 0.0f);
  assert(world.perceptual_primitive_summary.interaction_residue > 0.0f);
  assert(world.perceptual_primitive_summary.contact_field > 0.0f);
  assert(world.perceptual_primitive_summary.light_history > 0.0f);
  assert(world.perceptual_primitive_summary.acoustic_occlusion > 0.0f);
  assert(world.perceptual_primitive_summary.threat_gradient > 0.0f);
  assert(world.perceptual_primitive_summary.traversal_pressure > 0.0f);
  assert(world.perceptual_primitive_summary.player_readable_cause > 0.0f);
  assert(world.coal_mining_reaction.ai_attention_hash != 0u);
  assert(std::any_of(world.perceptual_primitives.begin(), world.perceptual_primitives.end(),
                     [](const aster::WorldPerceptualPrimitive &primitive) {
                       return primitive.accepted && primitive.sound_surface_class_hash != 0u &&
                              primitive.changed_channel_mask != 0u &&
                              primitive.decision_channel_mask != 0u &&
                              primitive.signals.light_history > 0.0f &&
                              primitive.signals.player_readable_cause > 0.0f;
                     }));
  return {.world_hash = world.world_hash,
          .graph_hash = world.perceptual_causality_graph.graph_hash,
          .primitive_truth_hash = world.perceptual_primitive_summary.truth_hash,
          .render_extraction_hash = world.render_extraction_hash,
          .audit_hash = world.world_truth_audit_hash,
          .changed_channel_mask = world.perceptual_causality_graph.changed_channel_mask,
          .decision_channel_mask = world.perceptual_causality_graph.decision_channel_mask,
          .decision_impact_score = world.perceptual_causality_graph.decision_impact_score};
}

void testLumenMineOreTorchDeterministicPerceptualReplay() {
  const LumenMineOreTorchReplayHashes first = runMineOreTorchReplay();
  const LumenMineOreTorchReplayHashes second = runMineOreTorchReplay();
  assert(first.world_hash == second.world_hash);
  assert(first.primitive_truth_hash == second.primitive_truth_hash);
  assert(first.belief_hash == second.belief_hash);
  assert(first.render_extraction_hash == second.render_extraction_hash);
  assert(first.ai_visibility_hash == second.ai_visibility_hash);
  assert(first.streaming_budget_hash == second.streaming_budget_hash);
  assert(first.audit_hash == second.audit_hash);
}

void testLumenPerceptualCausalityGraphReplay240() {
  const LumenPerceptualCausalityReplayProof first = runPerceptualCausalityReplay240();
  const LumenPerceptualCausalityReplayProof second = runPerceptualCausalityReplay240();
  assert(first.world_hash == second.world_hash);
  assert(first.graph_hash == second.graph_hash);
  assert(first.primitive_truth_hash == second.primitive_truth_hash);
  assert(first.render_extraction_hash == second.render_extraction_hash);
  assert(first.audit_hash == second.audit_hash);
  assert(first.changed_channel_mask == second.changed_channel_mask);
  assert(first.decision_channel_mask == second.decision_channel_mask);
  assert(first.decision_impact_score == second.decision_impact_score);
}

void testLumenPerceptualWorldRuntimeExposure() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::Vec3 cave_position = run.caveFrameReportPosition(18.0f);
  run.relocatePlayer(cave_position, run.caveFrameReportCameraYaw(18.0f));
  const int steps = static_cast<int>(std::ceil(47.0f / 0.05f));
  for (int i = 0; i < steps; ++i) {
    run.update(0.05f, {}, false, false);
  }
  const aster::PerceptualFrameState &state = run.worldForensics().perceptual_state;
  assert(state.accepted);
  assert(state.exposure_seconds >= 46.90f);
  assert(state.continuity_debt < 0.08f);
  assert(state.material_memory > 0.50f);
  assert(state.traversal_pressure > 0.45f);
  assert(state.lighting_believability > 0.45f);
  assert(state.occlusion_trust > 0.45f);
  assert(state.ecology_signal > 0.35f);
  assert(state.player_readable_cause > 0.45f);
  assert(state.semantic_budget_hash != 0u);
  const aster::PerceptualWorldScheduleReport &schedule =
      run.worldForensics().perceptual_schedule;
  assert(schedule.accepted);
  assert(schedule.belief_stability >= 0.70f);
  assert(schedule.streaming_budget > 0.0f);
  assert(schedule.decision_impact_score > 0.0f);
}

void testLumenCameraCollisionCanBeatComfortRadius() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0, .playable_radius = 86.0f});
  const float radius = run.resolveCameraRadius({0.0f, 1.0f, 85.0f}, 0.0f, 0.0f, 6.0f);
  assert(radius < 1.0f);
}

void testLumenInnerPondSeamHasSupport() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const auto &objects = run.scene().objects();

  const auto planar_half_extents = [](const aster::RenderObject &object) {
    aster::Vec2 half_extents{};
    if (object.custom_mesh == nullptr) {
      return half_extents;
    }
    for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
      half_extents.x =
          std::max(half_extents.x, std::abs(vertex.position.x * object.transform.scale.x));
      half_extents.y =
          std::max(half_extents.y, std::abs(vertex.position.z * object.transform.scale.z));
    }
    return half_extents;
  };

  const aster::RenderObject *largest_grass_surface = nullptr;
  float largest_grass_area = 0.0f;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::GrassSoil) {
      continue;
    }
    const aster::Vec2 half_extents = planar_half_extents(object);
    const float area = half_extents.x * half_extents.y;
    if (area > largest_grass_area) {
      largest_grass_area = area;
      largest_grass_surface = &object;
    }
  }
  assert(largest_grass_surface != nullptr);

  const aster::Vec2 seam_center{largest_grass_surface->transform.position.x,
                                largest_grass_surface->transform.position.z};
  const aster::RenderObject *seam_transition = nullptr;
  float closest_transition_distance = std::numeric_limits<float>::max();
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr ||
        object.material.surface_pattern != aster::SurfacePattern::TerrainBlend) {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - seam_center.x,
                            object.transform.position.z - seam_center.y};
    const float distance = aster::length(delta);
    if (distance < closest_transition_distance) {
      closest_transition_distance = distance;
      seam_transition = &object;
    }
  }
  assert(seam_transition != nullptr);
  const aster::Vec2 seam_radius = planar_half_extents(*seam_transition);
  assert(seam_radius.x > 1.0f && seam_radius.y > 1.0f);

  std::vector<const aster::RenderObject *> support_surfaces;
  for (const aster::RenderObject &object : objects) {
    if (object.custom_mesh == nullptr) {
      continue;
    }
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
      support_surfaces.push_back(&object);
      break;
    default:
      break;
    }
  }
  assert(!support_surfaces.empty());

  constexpr int angular_samples = 64;
  const float radial_samples[] = {0.84f, 0.95f, 1.06f};
  for (const float radial : radial_samples) {
    for (int i = 0; i < angular_samples; ++i) {
      const float angle =
          static_cast<float>(i) / static_cast<float>(angular_samples) * aster::radians(360.0f);
      const aster::Vec2 point{seam_center.x + std::cos(angle) * seam_radius.x * radial,
                              seam_center.y + std::sin(angle) * seam_radius.y * radial};
      aster::TerrainSurfaceSample best;
      for (const aster::RenderObject *surface : support_surfaces) {
        const aster::TerrainSurfaceSample sample = aster::sampleMeshSupport(
            *surface->custom_mesh, surface->transform, aster::SurfaceSupportQuery{point}, 0.25f);
        if (sample.valid && (!best.valid || sample.height > best.height)) {
          best = sample;
        }
      }
      assert(best.valid);
    }
  }
}

void testLumenSupportSurfacesRenderOpaque() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  bool saw_support_surface = false;

  for (const aster::RenderObject &object : run.scene().objects()) {
    switch (object.material.surface_pattern) {
    case aster::SurfacePattern::CourseCells:
    case aster::SurfacePattern::TerrainBlend:
    case aster::SurfacePattern::GrassSoil:
    case aster::SurfacePattern::SoilPath:
    case aster::SurfacePattern::LayeredTerrain:
      saw_support_surface = true;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(object.material.camera_occlusion == aster::CameraOcclusionPolicy::Solid);
      assert(!aster::isMaterialTranslucent(object.material));
      assert(aster::materialWritesDepth(object.material));
      assert(aster::isDoubleSidedMaterial(object.material));
      break;
    default:
      break;
    }
  }

  assert(saw_support_surface);
}

void testLumenSupplyCrateInventoryContract() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  assert(run.torchCount() == 0);
  assert(!run.supplyCrateNearby());
  assert(!run.takeSupplyTorch());

  const aster::Vec3 crate_position = run.supplyCratePosition();
  assert(crate_position.z < -40.0f);
  assert(aster::length({crate_position.x, 0.0f, crate_position.z}) > 45.0f);

  run.relocatePlayer(crate_position, 0.0f);
  assert(run.supplyCrateNearby());
  assert(run.takeSupplyTorch());
  assert(run.torchCount() == 1);
  assert(run.takeSupplyTorch());
  assert(run.torchCount() == 2);
}

void testLumenPrismRelayProximityInteraction() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const std::optional<aster::DynamicPointLight> idle_light = run.prismRelayLight();
  assert(idle_light.has_value());

  const aster::Vec3 base = run.prismRelayBasePosition();
  run.relocatePlayer(base + aster::Vec3{1.35f, 0.0f, 0.95f}, 0.0f);
  run.updateInteractionFocus({base.x, base.y + 8.0f, base.z}, {0.0f, 0.0f, 1.0f},
                             1.0f / 240.0f);
  const aster::FocusPromptModel prompt = run.focusPromptModel();
  assert(prompt.visible);
  assert(prompt.action == "Ignite");
  assert(prompt.subject == "Prism Relay");

  run.interactFocused();
  const std::optional<aster::DynamicPointLight> active_light = run.prismRelayLight();
  assert(active_light.has_value());
  assert(active_light->intensity > idle_light->intensity * 2.0f);
}

void testLumenCaveVisualContracts() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  run.relocatePlayer(run.supplyCratePosition(), 0.0f);
  for (int i = 0; i < 4; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  bool saw_cave_threshold = false;
  bool saw_cave_overhang = false;
  bool saw_cave_formation = false;
  bool saw_cave_liner = false;
  bool saw_cave_seal = false;
  bool saw_cave_throat = false;
  bool saw_authored_cave = false;
  bool saw_deep_cave = false;
  bool saw_central_grass_field = false;
  bool saw_cave_mouth_grass_field = false;
  bool saw_cave_web = false;
  aster::Vec3 cave_web_center{};
  int cave_skitter_count = 0;
  bool saw_coal_ore = false;
  bool saw_wall_light_lens = false;
  const aster::RenderObject *parkour_chest_base = nullptr;
  std::vector<const aster::RenderObject *> cave_shell_objects;
  int coal_ore_count = 0;

  for (const aster::RenderObject &object : run.scene().objects()) {
    if (startsWith(object.name, "Cave ")) {
      assert(object.primitive != aster::MeshPrimitive::Crystal);
    }
    if (object.name == "Engine central terrain grass field") {
      saw_central_grass_field = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 12000u);
      assert(object.material.surface_pattern == aster::SurfacePattern::Foliage);
    }
    if (object.name == "Engine cave mouth grass field") {
      saw_cave_mouth_grass_field = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 10000u);
      assert(object.material.surface_pattern == aster::SurfacePattern::Foliage);
    }
    if (startsWith(object.name, "Cave torch supply crate")) {
      assert(false);
    }
    if (object.name == "Walkable cave entrance threshold") {
      saw_cave_threshold = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.render_role == aster::MaterialRenderRole::SupportSurface);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);

      aster::Vec3 center{};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        center = center + vertex.position;
      }
      center = center / static_cast<float>(object.custom_mesh->vertices.size());
      const aster::TerrainSurfaceSample support =
          aster::sampleMeshSupport(*object.custom_mesh, object.transform,
                                   aster::SurfaceSupportQuery{{center.x, center.z}}, 0.36f);
      assert(support.valid);
    }
    if (object.name == "Smooth terrain blended cave overhang") {
      saw_cave_overhang = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.position.x, 0.0f, object.transform.position.z}) >
             45.0f);
    }
    if (object.name == "Continuous procedural cave mouth formation") {
      saw_cave_formation = true;
      assert(object.custom_mesh != nullptr);
      assert(object.custom_mesh->vertices.size() > 120u);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.material.procedural.micro_normal_strength > 0.0f);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.position.x, 0.0f, object.transform.position.z}) >
             45.0f);
    }
    if (object.name == "Opaque recessed cave mouth liner") {
      saw_cave_liner = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.double_sided);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.transform.position.z < -40.0f);
    }
    if (object.name == "Opaque cave portal terrain seal") {
      saw_cave_seal = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(countFacesOpposingVertexNormals(*object.custom_mesh) == 0u);
    }
    if (object.name == "Sealed cave entrance throat") {
      saw_cave_throat = true;
      assert(object.custom_mesh != nullptr);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(countFacesOpposingVertexNormals(*object.custom_mesh) == 0u);
    }
    assert(object.name != "Walkable packed cave floor");
    assert(object.name != "Walkable deep cave floor");
    if (object.name == "Authored cave interior") {
      saw_authored_cave = true;
      cave_shell_objects.push_back(&object);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.viewer_cull_volume.enabled);
      assert(object.viewer_cull_volume.outside == aster::FaceCullMode::Back);
      assert(object.viewer_cull_volume.inside == aster::FaceCullMode::Back);
      assert(object.custom_mesh != nullptr);
      aster::Vec3 center{};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        center = center + vertex.position;
      }
      center = center / static_cast<float>(object.custom_mesh->vertices.size());
      assert(center.z < -40.0f);
      assert(aster::length({center.x, 0.0f, center.z}) > 45.0f);
    }
    if (object.name == "Authored deep cave interior") {
      saw_deep_cave = true;
      cave_shell_objects.push_back(&object);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveRock);
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      assert(!object.dynamic_mesh.valid());
      assert(!object.camera_occlusion_fade);
    }
    assert(object.name.find("Opaque cave void") == std::string::npos);
    assert(object.name.find("Opaque deep cave void") == std::string::npos);
    assert(object.name.find("connector roof seal") == std::string::npos);
    assert(object.name.find("connector crown blocker") == std::string::npos);
    assert(object.name.find("green fungus") == std::string::npos);
    assert(object.name.find("Damp moss layer") == std::string::npos);
    if (object.name == "Continuous streaming cave connector shell" ||
        object.name == "Walkable streaming cave connector floor" ||
        object.name == "Chunked procedural cave interior" ||
        object.name == "Rock voxel cave surface" ||
        object.name == "Ironstone voxel cave surface") {
      assert(false);
    }
    if (object.name == "Oval cave spider web span") {
      saw_cave_web = true;
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      cave_web_center = {};
      for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
        cave_web_center = cave_web_center + vertex.position;
      }
      cave_web_center = cave_web_center / static_cast<float>(object.custom_mesh->vertices.size());
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveWeb);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Blend);
      assert(object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::None);
      assert(object.material.depth_policy.layer == aster::RenderDepthLayer::SurfaceAttachment);
      assert(object.material.depth_policy.constant_bias > 0.0f);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Cave skitter arachnid") {
      ++cave_skitter_count;
      assert(object.custom_mesh != nullptr);
      assert(!object.custom_mesh->vertices.empty());
      assert(object.material.surface_pattern == aster::SurfacePattern::CaveSkitterChitin);
      assert(object.material.opacity >= 0.999f);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(!object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.transform.position.z < -40.0f);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Coal ore vein node") {
      saw_coal_ore = true;
      ++coal_ore_count;
      assert(object.material.opacity >= 0.999f);
      assert(object.material.alpha_mode == aster::MaterialAlphaMode::Opaque);
      assert(object.material.depth_write == aster::MaterialDepthWrite::Enabled);
      assert(!object.material.double_sided);
      assert(object.material.cull_mode == aster::FaceCullMode::Back);
      assert(object.material.surface_pattern == aster::SurfacePattern::CoalVein);
      assert(object.material.emission_strength >= 0.10f);
      assert(object.primitive == aster::MeshPrimitive::Rock);
      assert(!object.viewer_cull_volume.enabled);
      assert(!object.camera_occlusion_fade);
      assert(object.transform.position.z < -40.0f);
      assert(aster::length({object.transform.scale.x, object.transform.scale.y,
                            object.transform.scale.z}) > 0.30f);
    }
    if (object.name == "Industrial amber cave wall light glowing lens") {
      saw_wall_light_lens = true;
      assert(object.material.surface_pattern == aster::SurfacePattern::AmberResin);
      assert(object.material.emission_strength > 0.60f);
      assert(object.material.emission_color.x > object.material.emission_color.z);
      assert(object.material.emission_color.y > object.material.emission_color.x * 0.52f);
      assert(object.material.emission_color.y < object.material.emission_color.x * 0.90f);
      assert(object.material.emission_color.z > object.material.emission_color.x * 0.30f);
      assert(object.material.emission_color.z < object.material.emission_color.x * 0.70f);
      assert(object.material.depth_policy.layer == aster::RenderDepthLayer::SurfaceAttachment);
      assert(!object.camera_occlusion_fade);
    }
    if (object.name == "Parkour starter chest base") {
      parkour_chest_base = &object;
    }
  }

  assert(saw_cave_threshold);
  assert(saw_cave_overhang);
  assert(saw_cave_formation);
  assert(saw_cave_liner);
  assert(saw_cave_seal);
  assert(saw_cave_throat);
  assert(saw_authored_cave);
  assert(saw_deep_cave);
  assert(saw_cave_web);
  assert(cave_skitter_count == 3);
  assert(saw_central_grass_field);
  assert(saw_cave_mouth_grass_field);
  assert(saw_coal_ore);
  assert(coal_ore_count >= 4);
  assert(saw_wall_light_lens);
  assert(parkour_chest_base != nullptr);
  assert(!cave_shell_objects.empty());
  const auto sample_visible_cave_support = [&](const aster::SurfaceSupportQuery &query) {
    aster::TerrainSurfaceSample best{};
    for (const aster::RenderObject *shell : cave_shell_objects) {
      assert(shell != nullptr);
      assert(shell->custom_mesh != nullptr);
      const aster::TerrainSurfaceSample shell_support =
          aster::sampleMeshSupport(*shell->custom_mesh, shell->transform, query, 0.30f);
      if (!best.valid || (shell_support.valid && shell_support.height > best.height)) {
        best = shell_support;
      }
    }
    return best;
  };
  for (const float progress : {8.0f, 16.0f, 24.0f, 32.0f}) {
    const aster::Vec3 cave_position = run.caveFrameReportPosition(progress);
    const aster::TerrainSurfaceSample visible_support =
        sample_visible_cave_support({{cave_position.x, cave_position.z},
                                     cave_position.y,
                                     0.10f,
                                     1.15f});
    assert(visible_support.valid);
    assert(visible_support.normal.y > 0.30f);
  }
  float chest_floor_height = -1000.0f;
  for (const aster::Vec3 local_offset :
       {aster::Vec3{0.0f, 0.0f, 0.0f},
        aster::Vec3{-parkour_chest_base->transform.scale.x, 0.0f,
                    -parkour_chest_base->transform.scale.z},
        aster::Vec3{-parkour_chest_base->transform.scale.x, 0.0f,
                    parkour_chest_base->transform.scale.z},
        aster::Vec3{parkour_chest_base->transform.scale.x, 0.0f,
                    -parkour_chest_base->transform.scale.z},
        aster::Vec3{parkour_chest_base->transform.scale.x, 0.0f,
                    parkour_chest_base->transform.scale.z}}) {
    const aster::Vec3 sample_position =
        parkour_chest_base->transform.position + aster::rotate(parkour_chest_base->transform.rotation,
                                                               local_offset);
    const aster::SurfaceSupportQuery query{{sample_position.x, sample_position.z},
                                           parkour_chest_base->transform.position.y + 0.50f,
                                           1.20f,
                                           4.0f};
    const aster::TerrainSurfaceSample chest_floor = sample_visible_cave_support(query);
    if (chest_floor.valid) {
      chest_floor_height = std::max(chest_floor_height, chest_floor.height);
    }
  }
  assert(chest_floor_height > -999.0f);
  assert(parkour_chest_base->transform.position.y - parkour_chest_base->transform.scale.y >=
         chest_floor_height - 0.015f);
}

void testLumenDeepCaveCaptureLightingContract() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const float progress = 16.0f;
  const aster::Vec3 player_position = run.caveFrameReportPosition(progress);
  run.relocatePlayer(player_position, 0.0f);
  for (int i = 0; i < 4; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }

  const aster::Vec3 look_target = run.caveFrameReportLookTarget(progress, 1.0f);
  const aster::CaveLightingState cave_light = run.caveLightingStateAt(look_target);
  assert(cave_light.interior > 0.30f);
  assert(cave_light.wall_light >= 0.0f && cave_light.wall_light <= 1.0f);
  assert(!cave_light.wall_lights.empty());

  aster::RendererSettings settings;
  settings.pipeline.clear_color = {0.001f, 0.001f, 0.001f};
  settings.exposure = 0.86f;
  settings.ambient_strength = 0.014f;
  settings.ambient_floor = 0.0f;
  settings.indirect_albedo_floor = 0.0f;
  settings.sky_ambient_color = {0.003f, 0.003f, 0.004f};
  settings.ground_ambient_color = {0.003f, 0.0025f, 0.002f};
  settings.sun_light.enabled = true;
  settings.sun_light.intensity = 0.0f;
  settings.sun_light.direction_to_light = {-0.46f, 0.86f, 0.30f};
  settings.atmosphere.enabled = false;
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    settings.light_rig.push_back({light.position, light.color, light.intensity, light.source_radius});
  }

  aster::OrbitCamera camera;
  camera.target = look_target;
  camera.pitch = aster::radians(6.0f);
  camera.yaw = aster::radians(180.0f);
  camera.radius = run.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, 2.70f);
  camera.vertical_fov = aster::radians(54.0f);

  aster::RenderDevice renderer;
  renderer.initialize();
  renderer.prepareScene(run.scene());
  (void)renderer.render(run.scene(), camera, settings, 96, 64, 0.0);

  const std::span<const std::uint8_t> rgba = aster::activeFrameBuffer().rgba8();
  double mean_luma = 0.0;
  std::size_t red_dominant_pixels = 0u;
  std::size_t bright_neutral_pixels = 0u;
  std::size_t green_dominant_pixels = 0u;
  for (std::size_t i = 0; i + 3u < rgba.size(); i += 4u) {
    const double r = static_cast<double>(rgba[i + 0u]) / 255.0;
    const double g = static_cast<double>(rgba[i + 1u]) / 255.0;
    const double b = static_cast<double>(rgba[i + 2u]) / 255.0;
    const double luma = r * 0.2126 + g * 0.7152 + b * 0.0722;
    mean_luma += luma;
    if (rgba[i + 0u] > 54u && rgba[i + 0u] > rgba[i + 1u] * 2u &&
        rgba[i + 0u] > rgba[i + 2u] * 2u) {
      ++red_dominant_pixels;
    }
    if (luma > 0.22 && std::abs(r - g) < 0.055 && std::abs(r - b) < 0.055) {
      ++bright_neutral_pixels;
    }
    if (luma > 0.035 && g > r * 1.20 && g > b * 1.20) {
      ++green_dominant_pixels;
    }
  }
  mean_luma /= static_cast<double>(std::max<std::size_t>(rgba.size() / 4u, 1u));
  assert(mean_luma > 0.08);
  assert(mean_luma < 0.42);
  assert(red_dominant_pixels < 2200u);
  assert(bright_neutral_pixels < 2800u);
  assert(green_dominant_pixels < 520u);

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testLumenHeldTorchLightsDeepCaveAndReplaysDeterministically() {
  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", true);
  setEnvFlag("ASTER_FORCE_NULL_RENDERER", false);

  struct LightingMetrics {
    double average_direct = 0.0;
    double max_source_readability = 0.0;
  };

  const float progress = 24.0f;
  const auto prepare_run = [&](aster::LumenRun &run, const bool with_torch) {
    if (with_torch) {
      assert(run.takeChestItem("torch"));
      run.selectHotbarSlot(0);
      run.update(1.0f / 60.0f, {}, false, false);
      assert(run.equippedLight().has_value());
    }
    const aster::Vec3 player_position = run.caveFrameReportPosition(progress);
    run.relocatePlayer(player_position, aster::radians(180.0f));
    for (int i = 0; i < 8; ++i) {
      run.update(1.0f / 60.0f, {}, false, false);
    }
  };

  const auto render_metrics = [&](const aster::LumenRun &run) {
    const aster::Vec3 look_target = run.caveFrameReportLookTarget(progress, 1.10f);
    const aster::CaveLightingState cave_light = run.caveLightingStateAt(look_target);
    aster::RendererSettings settings;
    settings.pipeline.clear_color = {0.001f, 0.001f, 0.001f};
    settings.exposure = 0.86f;
    settings.ambient_strength = 0.010f;
    settings.ambient_floor = 0.0f;
    settings.indirect_albedo_floor = 0.0f;
    settings.sky_ambient_color = {0.0025f, 0.0025f, 0.0030f};
    settings.ground_ambient_color = {0.0025f, 0.0020f, 0.0018f};
    settings.sun_light.enabled = true;
    settings.sun_light.intensity = 0.0f;
    settings.atmosphere.enabled = false;
    for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
      settings.light_rig.push_back({light.position, light.color, light.intensity,
                                    light.source_radius});
    }
    if (const std::optional<aster::DynamicPointLight> light = run.equippedLight();
        light.has_value() && light->active) {
      const float gain = run.heldTorchLightGain(cave_light);
      settings.light_rig.push_back(
          {light->position, light->color, light->intensity * gain, light->source_radius});
    }

    aster::OrbitCamera camera;
    camera.target = look_target;
    camera.pitch = aster::radians(6.0f);
    camera.yaw = aster::radians(180.0f);
    camera.radius = run.resolveCameraRadius(camera.target, camera.yaw, camera.pitch, 2.55f);
    camera.vertical_fov = aster::radians(54.0f);

    const aster::SoftwarePreviewResult result =
        aster::renderSoftwarePreviewWithProbe(run.scene(), camera,
                                              {.width = 96,
                                               .height = 64,
                                               .samples_per_axis = 1,
                                               .frame_seconds = 0.0,
                                               .settings = settings});
    LightingMetrics metrics;
    std::size_t lit_pixels = 0u;
    for (const aster::SoftwareLightingProbePixel &pixel : result.lighting.pixels) {
      metrics.average_direct += pixel.direct_light_luminance;
      metrics.max_source_readability =
          std::max(metrics.max_source_readability,
                   static_cast<double>(pixel.source_readability_luminance));
      if (pixel.direct_light_luminance > 0.0001f) {
        ++lit_pixels;
      }
    }
    metrics.average_direct /=
        static_cast<double>(std::max<std::size_t>(result.lighting.pixels.size(), 1u));
    assert(lit_pixels > 0u);
    return metrics;
  };

  aster::LumenRun no_torch({.shard_count = 3, .sentinel_count = 0});
  aster::LumenRun torch_a({.shard_count = 3, .sentinel_count = 0});
  aster::LumenRun torch_b({.shard_count = 3, .sentinel_count = 0});
  prepare_run(no_torch, false);
  prepare_run(torch_a, true);
  prepare_run(torch_b, true);
  const LightingMetrics baseline = render_metrics(no_torch);
  const LightingMetrics held = render_metrics(torch_a);
  (void)render_metrics(torch_b);
  assert(held.average_direct > baseline.average_direct * 1.08 + 0.0001);
  assert(held.max_source_readability > baseline.max_source_readability + 0.0001);
  assert(torch_a.worldForensics().perceptual_primitive_summary.truth_hash ==
         torch_b.worldForensics().perceptual_primitive_summary.truth_hash);
  assert(std::any_of(torch_a.worldForensics().perceptual_primitives.begin(),
                     torch_a.worldForensics().perceptual_primitives.end(),
                     [](const aster::WorldPerceptualPrimitive &primitive) {
                       return primitive.primitive_id.find("lumen.torch.exposure.surface.") == 0u &&
                              primitive.accepted && primitive.signals.light_history > 0.0f &&
                              primitive.neural_irradiance_hash != 0u;
                     }));

  setEnvFlag("ASTER_FORCE_SOFTWARE_RENDERER", false);
}

void testLumenCaveTraversalAndLightingContracts() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  aster::Vec3 cave_web_center{};
  bool found_cave_web = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name != "Oval cave spider web span") {
      continue;
    }
    assert(object.custom_mesh != nullptr);
    for (const aster::Vertex &vertex : object.custom_mesh->vertices) {
      cave_web_center = cave_web_center + vertex.position;
    }
    cave_web_center = cave_web_center / static_cast<float>(object.custom_mesh->vertices.size());
    found_cave_web = true;
    break;
  }
  assert(found_cave_web);
  const auto require_lumen_transition = [](const bool condition, const std::string &message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  };
  run.relocatePlayer({31.0f, 0.80f, -57.80f}, aster::radians(180.0f));
  const int entrance_recovery_lives = run.status().lives;
  const aster::Vec3 entrance_recovery_target{31.0f, 7.0f, -70.0f};
  for (int frame = 0; frame < 210; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{entrance_recovery_target.x - position.x,
                          entrance_recovery_target.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 entrance_recovered_position = run.playerPosition();
  require_lumen_transition(run.status().lives == entrance_recovery_lives,
                           "cave entrance floor recovery respawned the player");
  require_lumen_transition(aster::length({entrance_recovered_position.x, 0.0f,
                                          entrance_recovered_position.z}) > 55.0f,
                           "cave entrance floor recovery dropped the player below world bounds");
  require_lumen_transition(entrance_recovered_position.y > 5.0f,
                           "cave entrance floor recovery failed to snap back to the threshold");

  const aster::Vec3 web_approach_position =
      cave_web_center + aster::Vec3{0.0f, -0.10f, 2.35f};
  run.relocatePlayer(web_approach_position, aster::radians(180.0f));
  const int health_before_web = run.status().health;
  for (int i = 0; i < 90; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  assert(run.status().health == health_before_web);
  const aster::Vec3 focus_origin = web_approach_position + aster::Vec3{0.0f, 0.42f, 0.0f};
  require_lumen_transition(run.takeChestItem("pickaxe"),
                           "test setup failed to equip the pickaxe for cave web mining");
  run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                             1.0f / 60.0f);
  const aster::FocusPromptModel transition_prompt = run.focusPromptModel();
  require_lumen_transition(transition_prompt.visible, "cave transition web prompt is not visible");
  require_lumen_transition(transition_prompt.action == "Cut",
                           "cave transition should focus the web before attached skitters; got " +
                               transition_prompt.action + " " + transition_prompt.subject);
  require_lumen_transition(transition_prompt.subject == "Spider Web",
                           "cave transition should not focus rock before the web is cut");
  require_lumen_transition(!(transition_prompt.visible && transition_prompt.action == "Strike" &&
                             transition_prompt.subject == "Rock"),
                           "cave transition exposed a rock prompt before the web was cut");
  bool web_cut_prompt_seen = false;
  bool web_cleared = false;
  for (int swing = 0; swing < 8; ++swing) {
    run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                               1.0f / 60.0f);
    const aster::FocusPromptModel prompt = run.focusPromptModel();
    if (prompt.visible && prompt.action == "Cut" && prompt.subject == "Spider Web") {
      web_cut_prompt_seen = true;
    } else if (web_cut_prompt_seen) {
      web_cleared = true;
      break;
    }
    run.interactFocused();
    for (int frame = 0; frame < 40; ++frame) {
      run.update(1.0f / 60.0f, {}, false, false);
    }
  }
  run.updateInteractionFocus(focus_origin, aster::normalize(cave_web_center - focus_origin),
                             1.0f / 60.0f);
  const aster::FocusPromptModel cleared_prompt = run.focusPromptModel();
  web_cleared = web_cleared || !(cleared_prompt.visible && cleared_prompt.action == "Cut" &&
                                 cleared_prompt.subject == "Spider Web");
  require_lumen_transition(web_cut_prompt_seen, "cave transition web was never interactable");
  require_lumen_transition(web_cleared, "cave transition web remained focused after mining");
  require_lumen_transition(!(cleared_prompt.visible && cleared_prompt.action == "Strike" &&
                             cleared_prompt.subject == "Rock"),
                           "cave transition exposed a rock prompt immediately after web mining");

  const aster::Vec3 traversal_target = run.caveFrameReportPosition(16.0f);
  run.relocatePlayer(traversal_target + aster::Vec3{0.0f, 2.25f, 0.0f},
                     run.caveFrameReportCameraYaw(16.0f));
  const aster::Vec3 snapped_cave_position = run.playerPosition();
  require_lumen_transition(std::abs(snapped_cave_position.y - traversal_target.y) < 0.45f,
                           "player snapped to an upper terrain/roof surface instead of the cave floor");
  for (const float progress : {8.0f, 16.0f, 24.0f, 32.0f}) {
    const aster::Vec3 expected_floor = run.caveFrameReportPosition(progress);
    run.relocatePlayer(expected_floor + aster::Vec3{0.0f, 2.25f, 0.0f},
                       run.caveFrameReportCameraYaw(progress));
    const aster::Vec3 snapped = run.playerPosition();
    const aster::Vec2 planar_delta{snapped.x - expected_floor.x, snapped.z - expected_floor.z};
    require_lumen_transition(aster::length(planar_delta) < 0.35f,
                             "deep cave relocation drifted to another chunk footprint");
    require_lumen_transition(std::abs(snapped.y - expected_floor.y) < 0.45f,
                             "deep cave relocation chose hidden shell support instead of the visible floor");
    require_lumen_transition(aster::length({snapped.x, 0.0f, snapped.z}) > 40.0f,
                             "deep cave relocation reset the player toward the spawn arena");
  }
  const auto planar_distance_to_target = [&](const aster::Vec3 position) {
    const aster::Vec2 delta{traversal_target.x - position.x, traversal_target.z - position.z};
    return aster::length(delta);
  };
  const aster::Vec3 approach_to_target = aster::normalize(traversal_target - cave_web_center);
  run.relocatePlayer(web_approach_position, aster::radians(180.0f));
  const float initial_target_distance = planar_distance_to_target(run.playerPosition());
  for (int frame = 0; frame < 520; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{traversal_target.x - position.x, traversal_target.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 traversed_position = run.playerPosition();
  require_lumen_transition(planar_distance_to_target(traversed_position) <
                               initial_target_distance - 6.0f,
                           "player did not advance through the cleared cave connector");
  require_lumen_transition(aster::dot(traversed_position - cave_web_center, approach_to_target) >
                               4.0f,
                           "player remained on the authored side of the cave web");
  require_lumen_transition(traversed_position.z < cave_web_center.z - 4.0f,
                           "player did not move past the web into the authored deep cave");
  const auto planar_distance_to_approach = [&](const aster::Vec3 position) {
    const aster::Vec2 delta{web_approach_position.x - position.x,
                            web_approach_position.z - position.z};
    return aster::length(delta);
  };
  const float return_initial_distance = planar_distance_to_approach(traversed_position);
  for (int frame = 0; frame < 420; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{web_approach_position.x - position.x,
                          web_approach_position.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 returned_position = run.playerPosition();
  require_lumen_transition(planar_distance_to_approach(returned_position) <
                               return_initial_distance - 3.0f,
                           "player could not backtrack through the cave connector");

  const aster::Vec3 deep_chunk_target = run.caveFrameReportPosition(32.0f);
  const auto planar_distance_to_deep_chunk = [&](const aster::Vec3 position) {
    const aster::Vec2 delta{deep_chunk_target.x - position.x, deep_chunk_target.z - position.z};
    return aster::length(delta);
  };
  run.relocatePlayer(web_approach_position, aster::radians(180.0f));
  const float deep_initial_distance = planar_distance_to_deep_chunk(run.playerPosition());
  for (int frame = 0; frame < 1120; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{deep_chunk_target.x - position.x, deep_chunk_target.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 deep_chunk_position = run.playerPosition();
  require_lumen_transition(aster::length({deep_chunk_position.x, 0.0f, deep_chunk_position.z}) >
                               80.0f,
                           "deep cave traversal crossed the world-bounds guard and respawned");
  require_lumen_transition(planar_distance_to_deep_chunk(deep_chunk_position) <
                               deep_initial_distance - 12.0f,
                           "deep cave traversal stopped on the wrong chunk support surface");
  require_lumen_transition(deep_chunk_position.y < web_approach_position.y - 1.25f,
                           "deep cave traversal climbed onto upper terrain instead of descending");

  const float deep_return_initial_distance = planar_distance_to_approach(deep_chunk_position);
  for (int frame = 0; frame < 900; ++frame) {
    const aster::Vec3 position = run.playerPosition();
    aster::Vec2 move_axis{web_approach_position.x - position.x,
                          web_approach_position.z - position.z};
    const float move_length = aster::length(move_axis);
    if (move_length > 0.001f) {
      move_axis = move_axis / move_length;
    }
    run.update(1.0f / 60.0f, move_axis, true, false);
  }
  const aster::Vec3 deep_returned_position = run.playerPosition();
  require_lumen_transition(planar_distance_to_approach(deep_returned_position) <
                               deep_return_initial_distance - 8.0f,
                           "player could not backtrack after crossing into a deeper cave chunk");
  const aster::CaveLightingState spawn_cave_light = run.caveLightingStateAt({0.0f, 0.32f, 0.0f});
  assert(spawn_cave_light.interior < 0.001f);
  assert(spawn_cave_light.entrance_light < 0.001f);
  for (const aster::CaveWallLightSample &light : spawn_cave_light.wall_lights) {
    assert(light.intensity <= 0.001f);
  }
  const aster::CaveLightingState cave_light = run.caveLightingState();
  assert(cave_light.interior > 0.20f);
  assert(!cave_light.wall_lights.empty());
  bool saw_readable_fixture_light = false;
  aster::CaveWallLightSample readable_fixture_light{};
  for (const aster::CaveWallLightSample &light : cave_light.wall_lights) {
    const bool readable_fixture_color =
        light.color.x > 0.90f && light.color.y > light.color.x * 0.52f &&
        light.color.y < light.color.x * 0.90f && light.color.z > light.color.x * 0.30f &&
        light.color.z < light.color.x * 0.70f;
    if (readable_fixture_color && light.intensity > 6.0f && light.source_radius > 0.0f &&
        light.source_radius <= 2.40f) {
      saw_readable_fixture_light = true;
      readable_fixture_light = light;
      break;
    }
  }
  assert(saw_readable_fixture_light);
  assert(cave_light.wall_light >= 0.0f && cave_light.wall_light <= 1.0f);
  const aster::CaveLightingState near_fixture_light =
      run.caveLightingStateAt(readable_fixture_light.position);
  assert(near_fixture_light.wall_light > spawn_cave_light.wall_light + 0.05f);

  run.reset();
  const aster::CaveLightingState reset_spawn_light = run.caveLightingState();
  assert(reset_spawn_light.interior < 0.001f);
  assert(reset_spawn_light.wall_light < 0.001f);
  assert(reset_spawn_light.wall_lights.empty());
}

void testLumenPondWallLightIsMountedOutsideWater() {
  const aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  const aster::RenderObject *inner_water = nullptr;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Deep inner castle pond water") {
      inner_water = &object;
      break;
    }
  }
  assert(inner_water != nullptr);
  assert(inner_water->custom_mesh != nullptr);

  aster::Vec2 water_radius{};
  for (const aster::Vertex &vertex : inner_water->custom_mesh->vertices) {
    water_radius.x =
        std::max(water_radius.x, std::abs(vertex.position.x * inner_water->transform.scale.x));
    water_radius.y =
        std::max(water_radius.y, std::abs(vertex.position.z * inner_water->transform.scale.z));
  }
  assert(water_radius.x > 1.0f && water_radius.y > 1.0f);

  const aster::Vec2 water_center{inner_water->transform.position.x,
                                 inner_water->transform.position.z};
  bool saw_wall_fixture = false;
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name != "Industrial amber cave wall light glowing lens") {
      continue;
    }
    const aster::Vec2 delta{object.transform.position.x - water_center.x,
                            object.transform.position.z - water_center.y};
    if (std::abs(delta.x) > water_radius.x + 4.0f || std::abs(delta.y) > water_radius.y + 4.0f) {
      continue;
    }
    saw_wall_fixture = true;
    const float normalized_footprint =
        std::sqrt((delta.x * delta.x) / (water_radius.x * water_radius.x) +
                  (delta.y * delta.y) / (water_radius.y * water_radius.y));
    assert(normalized_footprint > 1.18f);
    assert(object.transform.position.y > inner_water->transform.position.y + 0.70f);
  }
  assert(saw_wall_fixture);
}

void testLumenClassicGauntletVisibleAndAutomapped() {
  aster::LumenRun run({.shard_count = 3, .sentinel_count = 0});
  bool saw_bulkhead = false;
  bool saw_lift = false;
  bool saw_console = false;
  bool saw_encounter_actor = false;
  aster::Vec3 left_door_before{};
  aster::Vec3 right_door_before{};
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Classic gauntlet prism bulkhead left") {
      saw_bulkhead = true;
      left_door_before = object.transform.position;
      assert(object.material.emission_strength > 0.0f);
    }
    if (object.name == "Classic gauntlet prism bulkhead right") {
      right_door_before = object.transform.position;
    }
    saw_lift = saw_lift || object.name == "Classic gauntlet lift platform";
    saw_console = saw_console || object.name == "Classic gauntlet automap console";
    saw_encounter_actor =
        saw_encounter_actor || object.name == "Classic gauntlet encounter sentinel";
  }
  assert(saw_bulkhead);
  assert(saw_lift);
  assert(saw_console);
  assert(saw_encounter_actor);

  run.relocatePlayer(run.classicGauntletEntryPosition(), run.classicGauntletCameraYaw());
  for (int i = 0; i < 140; ++i) {
    run.update(1.0f / 60.0f, {}, false, false);
  }
  assert(run.classicGauntletActive());
  assert(run.classicGauntletAutomap().hasDiscovery());
  assert(run.classicHudSignals().visible);

  aster::Vec3 left_door_after{};
  aster::Vec3 right_door_after{};
  for (const aster::RenderObject &object : run.scene().objects()) {
    if (object.name == "Classic gauntlet prism bulkhead left") {
      left_door_after = object.transform.position;
    }
    if (object.name == "Classic gauntlet prism bulkhead right") {
      right_door_after = object.transform.position;
    }
  }
  assert(aster::length(left_door_after - right_door_after) >
         aster::length(left_door_before - right_door_before) + 0.25f);
  assert(!run.classicTransitionWipe().column_progress.empty());
}

} // namespace

struct NamedSampleTest {
  const char *name = "";
  void (*run)() = nullptr;
};

bool sampleTestSelected(const char *name, const int argc, const char **argv) {
  if (argc <= 1) {
    return true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], name) == 0) {
      return true;
    }
  }
  return false;
}

int main(const int argc, const char **argv) {
  const NamedSampleTest tests[] = {
      {"lumen_scene_coherence_report", testLumenSceneCoherenceReport},
      {"lumen_world_forensics_contract", testLumenWorldForensicsContract},
      {"lumen_coal_mining_reaction_continuity", testLumenCoalMiningReactionContinuity},
      {"lumen_world_renderable_perceptual_coverage", testLumenWorldRenderablePerceptualCoverage},
      {"lumen_mine_ore_torch_deterministic_perceptual_replay",
       testLumenMineOreTorchDeterministicPerceptualReplay},
      {"lumen_perceptual_causality_graph_replay_240",
       testLumenPerceptualCausalityGraphReplay240},
      {"lumen_perceptual_world_runtime_exposure", testLumenPerceptualWorldRuntimeExposure},
      {"lumen_camera_collision_can_beat_comfort_radius",
       testLumenCameraCollisionCanBeatComfortRadius},
      {"lumen_inner_pond_seam_has_support", testLumenInnerPondSeamHasSupport},
      {"lumen_support_surfaces_render_opaque", testLumenSupportSurfacesRenderOpaque},
      {"lumen_supply_crate_inventory_contract", testLumenSupplyCrateInventoryContract},
      {"lumen_prism_relay_proximity_interaction", testLumenPrismRelayProximityInteraction},
      {"lumen_cave_visual_contracts", testLumenCaveVisualContracts},
      {"lumen_deep_cave_capture_lighting_contract", testLumenDeepCaveCaptureLightingContract},
      {"lumen_held_torch_lights_deep_cave_and_replays_deterministically",
       testLumenHeldTorchLightsDeepCaveAndReplaysDeterministically},
      {"lumen_cave_traversal_and_lighting_contracts",
       testLumenCaveTraversalAndLightingContracts},
      {"lumen_pond_wall_light_is_mounted_outside_water", testLumenPondWallLightIsMountedOutsideWater},
      {"lumen_classic_gauntlet_visible_and_automapped",
       testLumenClassicGauntletVisibleAndAutomapped},
  };
  bool ran = false;
  for (const NamedSampleTest &test : tests) {
    if (!sampleTestSelected(test.name, argc, argv)) {
      continue;
    }
    test.run();
    ran = true;
  }
  assert(ran);
  std::cout << "sample_tests passed.\n";
  return 0;
}
