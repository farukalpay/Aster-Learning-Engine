// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "lumen_run_detail.hpp"

namespace aster {
namespace {

constexpr std::uint64_t kLumenWorldSeed64 = 0xA57E4C554D454E57ull;
constexpr std::uint64_t kLumenEntryRegionId = 0x4C554D454E434156ull;
constexpr std::uint32_t kContinuitySpatialAffordance = 1u << 0u;
constexpr std::uint32_t kContinuityMotionContinuity = 1u << 1u;
constexpr std::uint32_t kContinuityHazardReadability = 1u << 2u;
constexpr std::uint32_t kContinuityMaterialMemory = 1u << 3u;
constexpr std::uint32_t kContinuityLightingAtmosphere = 1u << 4u;
constexpr std::uint32_t kContinuityEventResidue = 1u << 5u;
constexpr std::uint32_t kContinuitySensoryFeedback = 1u << 6u;
constexpr std::uint32_t kContinuityAiAttention = 1u << 7u;
constexpr std::uint32_t kContinuityStreamingResidency = 1u << 8u;
constexpr std::uint32_t kContinuityUiFeedback = 1u << 9u;
constexpr std::uint32_t kContinuityResourceState = 1u << 10u;

[[nodiscard]] std::uint64_t lumenHash(std::uint64_t seed, const std::uint64_t value) {
  return hashCombine64(seed == 0u ? kLumenWorldSeed64 : seed, value);
}

[[nodiscard]] std::uint64_t lumenHash(const bool value, std::uint64_t seed) {
  return lumenHash(seed, value ? 0xF00DCAFEu : 0xBAD5EEDu);
}

[[nodiscard]] std::uint64_t lumenHash(const float value, std::uint64_t seed) {
  return lumenHash(seed, static_cast<std::uint64_t>(stableHash32(value)));
}

[[nodiscard]] std::uint64_t lumenHash(const Vec2 value, std::uint64_t seed) {
  seed = lumenHash(value.x, seed);
  return lumenHash(value.y, seed);
}

[[nodiscard]] std::uint64_t lumenHash(const Vec3 value, std::uint64_t seed) {
  seed = lumenHash(value.x, seed);
  seed = lumenHash(value.y, seed);
  return lumenHash(value.z, seed);
}

[[nodiscard]] std::uint64_t lumenHashString(const std::string_view text, std::uint64_t seed) {
  for (const char c : text) {
    seed = lumenHash(seed, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
  }
  return seed;
}

[[nodiscard]] std::uint32_t lumenContinuityChannelMask(const std::string_view channel) {
  if (channel == "spatial_affordance") {
    return kContinuitySpatialAffordance;
  }
  if (channel == "motion_continuity") {
    return kContinuityMotionContinuity;
  }
  if (channel == "hazard_readability") {
    return kContinuityHazardReadability;
  }
  if (channel == "material_memory") {
    return kContinuityMaterialMemory;
  }
  if (channel == "lighting_atmosphere") {
    return kContinuityLightingAtmosphere;
  }
  if (channel == "event_residue") {
    return kContinuityEventResidue;
  }
  if (channel == "sensory_feedback") {
    return kContinuitySensoryFeedback;
  }
  if (channel == "ai_attention") {
    return kContinuityAiAttention;
  }
  if (channel == "streaming_residency") {
    return kContinuityStreamingResidency;
  }
  if (channel == "ui_feedback") {
    return kContinuityUiFeedback;
  }
  if (channel == "resource_state") {
    return kContinuityResourceState;
  }
  return 0u;
}

[[nodiscard]] std::uint32_t
lumenContinuityChannelMask(const std::vector<std::string> &channels) {
  std::uint32_t mask = 0u;
  for (const std::string &channel : channels) {
    mask |= lumenContinuityChannelMask(channel);
  }
  return mask;
}

[[nodiscard]] std::uint32_t lumenPerceptionLedgerDefaultMask() {
  return worldPerceptionLedgerChannelBit("material_memory") |
         worldPerceptionLedgerChannelBit("contact_history") |
         worldPerceptionLedgerChannelBit("lighting_exposure") |
         worldPerceptionLedgerChannelBit("atmosphere_cell") |
         worldPerceptionLedgerChannelBit("occlusion_role") |
         worldPerceptionLedgerChannelBit("gameplay_affordance") |
         worldPerceptionLedgerChannelBit("wear_continuity") |
         worldPerceptionLedgerChannelBit("streaming_semantic_lod") |
         worldPerceptionLedgerChannelBit("audio_visual_cue_budget");
}

[[nodiscard]] std::uint32_t
lumenPerceptionLedgerChannelMask(const std::vector<std::string> &channels) {
  std::uint32_t mask = 0u;
  for (const std::string &channel : channels) {
    mask |= worldPerceptionLedgerChannelBit(channel);
  }
  return mask;
}

[[nodiscard]] float lumenContinuityScore(const std::uint32_t required,
                                         const std::uint32_t observed) {
  if (required == 0u) {
    return 1.0f;
  }
  return static_cast<float>(std::popcount(required & observed)) /
         static_cast<float>(std::popcount(required));
}

[[nodiscard]] Vec3 lumenSdkVec(const sdk::Vec3 &value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] float lumenRouteLength(const std::vector<Vec3> &points) {
  float route_length = 0.0f;
  for (std::size_t i = 1u; i < points.size(); ++i) {
    route_length += length(points[i] - points[i - 1u]);
  }
  return route_length;
}

[[nodiscard]] std::string appendGateReason(std::string current, const std::string_view reason) {
  if (!current.empty()) {
    current += "; ";
  }
  current += reason;
  return current;
}

[[nodiscard]] const char *lumenGateVerdictName(const LumenWorldGateVerdict verdict) {
  switch (verdict) {
  case LumenWorldGateVerdict::Unknown:
    return "unknown";
  case LumenWorldGateVerdict::Accepted:
    return "accepted";
  case LumenWorldGateVerdict::Quarantined:
    return "quarantined";
  }
  return "unknown";
}

} // namespace

// Public LumenRun lifecycle and app-facing query/update surface.
LumenRun::LumenRun(LumenTuning tuning) : tuning_(tuning) {
  reset();
}

LumenRun::LumenRun(LumenAuthoringData authoring, LumenTuning tuning)
    : tuning_(tuning), authoring_(std::move(authoring)) {
  reset();
}

void LumenRun::reset() {
  ASTER_PROFILE_SCOPE("LumenRun::reset");
  resetWorldProof();
  status_ = {};
  status_.lives = 3;
  status_.max_health = kPlayerMaxHealth;
  status_.health = status_.max_health;
  status_.total_shards = tuning_.shard_count;
  terrain_ = makeProceduralTerrain({.grid_size = 289,
                                    .square_size = 0.74f,
                                    .central_flat_radius = tuning_.arena_radius * 1.06f,
                                    .transition_width = 10.0f,
                                    .hill_height = 1.18f,
                                    .mountain_height = 3.10f});
  sculptLumenCaveTerrain(terrain_);
  const TerrainSurfaceSample start_ground = sampleTerrain(terrain_, {0.0f, 0.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  prism_relay_base_ = prismRelayBasePlacement();
  prism_relay_active_ = false;
  prism_relay_charge_ = kPrismRelayIdleCharge;
  player_velocity_ = {};
  player_avatar_instance_ = {};
  player_facing_yaw_ = 0.0f;
  player_avatar_pose_ = {};
  player_preview_yaw_ = 0.0f;
  player_preview_yaw_enabled_ = false;
  player_avatar_animator_ = {};
  player_avatar_support_extent_ = 0.0f;
  player_render_position_ = player_position_;
  player_avatar_pose_valid_ = false;
  player_render_position_valid_ = false;
  player_avatar_point_target_ = {};
  player_avatar_point_enabled_ = false;
  player_mouth_open_ = 0.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  invulnerability_ = 0.0f;
  death_state_ = DeathSequenceState::Alive;
  death_timer_ = 0.0f;
  death_origin_ = {};
  pending_defeat_ = false;
  item_registry_ = makeLumenRunItemRegistry();
  chest_inventory_ = InventoryContainer(3u);
  hotbar_ = Hotbar(6u);
  equipment_.clear();
  for (const char *item_id : {"pickaxe", "torch", "fishing_rod"}) {
    if (const ItemDefinition *definition = item_registry_.find(item_id)) {
      (void)chest_inventory_.addItem(*definition, 1);
    }
  }
  interaction_.clear();
  chest_lid_animation_.snap(0.0f);
  chest_open_ = false;
  chest_selected_slot_ = 0u;
  supply_crate_base_ = {};
  supply_crate_yaw_ = 0.0f;
  equipped_light_ = {};
  prism_relay_core_object_ = 0;
  prism_relay_core_valid_ = false;
  prism_relay_ring_objects_.clear();
  prism_relay_conduit_objects_.clear();
  prism_relay_node_objects_.clear();
  torch_flame_.reset();
  crocodile_state_ = {};
  crocodile_state_.position = inner_pond_center_ + Vec3{-2.80f, 0.24f, 0.18f};
  crocodile_state_.facing_yaw = radians(78.0f);
  crocodile_swim_blend_ = 1.0f;

  shards_.clear();
  sentinels_.clear();
  rim_objects_.clear();
  scenery_objects_.clear();
  fishing_line_objects_.clear();
  fish_.clear();
  castle_birds_.clear();
  crocodile_objects_.clear();
  blood_particles_.clear();
  blood_particle_cursor_ = 0;
  chest_items_.clear();
  equipped_item_parts_.clear();
  placed_rocks_.clear();
  scenery_collision_boxes_.clear();
  torch_particle_visuals_.clear();
  mining_fracture_shards_.clear();
  coal_ores_.clear();
  cave_webs_.clear();
  cave_skitters_.clear();
  cave_sections_.clear();
  classic_gauntlet_mechanisms_.clear();
  classic_gauntlet_actors_.clear();
  classic_gauntlet_automap_.clear();
  classic_gauntlet_wipe_.finish();
  classic_gauntlet_hud_ = {};
  classic_gauntlet_entry_ = {};
  classic_gauntlet_plate_ = {};
  classic_gauntlet_door_center_ = {};
  classic_gauntlet_door_side_ = {1.0f, 0.0f, 0.0f};
  classic_gauntlet_lift_base_ = {};
  classic_gauntlet_exit_ = {};
  classic_gauntlet_door_objects_.clear();
  classic_gauntlet_signal_objects_.clear();
  classic_gauntlet_actor_visuals_.clear();
  classic_gauntlet_lift_object_ = 0;
  classic_gauntlet_plate_object_ = 0;
  classic_gauntlet_map_object_ = 0;
  classic_gauntlet_active_ = false;
  classic_gauntlet_discovered_ = false;
  classic_gauntlet_wipe_started_ = false;
  classic_gauntlet_hurt_seconds_ = 0.0f;
  cave_collision_meshes_.clear();
  cave_floor_supports_.clear();
  cave_exterior_hidden_objects_.clear();
  cave_viewer_cull_volume_ = {};
  cave_debug_overlay_objects_.clear();
  focused_cave_web_index_ = 0;
  focused_cave_web_valid_ = false;
  mining_.reset();
  placed_resource_serial_ = 1u;
  x_eye_objects_.clear();
  eye_objects_valid_ = false;
  const Vec2 climbable_tree_planar{-22.0f, 15.8f};
  const TerrainSurfaceSample climbable_tree_ground = sampleTerrain(terrain_, climbable_tree_planar);
  climbable_tree_ = {{climbable_tree_planar.x,
                      (climbable_tree_ground.valid ? climbable_tree_ground.height : 0.0f) + 0.03f,
                      climbable_tree_planar.y},
                     0.46f,
                     3.70f};
  player_grounded_ = false;
  forced_spawn_lighting_frames_ = 2;

  const float golden_angle = kPi * (3.0f - std::sqrt(5.0f));
  const auto shard_overlaps_reserved_composition = [](const Vec3 position) {
    return overlapsFishingComposition(position) || overlapsParkourChestComposition(position);
  };
  const auto relocate_shard = [&](Shard &shard, const int seed) {
    Vec2 planar = normalize(Vec2{shard.position.x, shard.position.z});
    if (length(planar) <= 0.0001f) {
      planar = {1.0f, 0.0f};
    }
    const Vec2 rotated{-planar.y, planar.x};
    for (int attempt = 0; attempt < 8; ++attempt) {
      const float side_bias = 0.54f + static_cast<float>(attempt) * 0.11f;
      const float radius_scale = 0.64f + static_cast<float>((seed + attempt) % 3) * 0.07f;
      const Vec2 relocated =
          normalize(planar * 0.48f + rotated * side_bias) * (tuning_.arena_radius * radius_scale);
      shard.position.x = relocated.x;
      shard.position.z = relocated.y;
      if (!shard_overlaps_reserved_composition(shard.position)) {
        return;
      }
      planar = normalize(Vec2{std::cos(static_cast<float>(seed + attempt + 1) * golden_angle),
                              std::sin(static_cast<float>(seed + attempt + 1) * golden_angle)});
    }
  };
  for (int i = 0; i < tuning_.shard_count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(tuning_.shard_count);
    const float radius = std::sqrt(fill) * tuning_.arena_radius * 0.82f;
    const float angle = static_cast<float>(i) * golden_angle;
    Shard shard;
    shard.position = {std::cos(angle) * radius, tuning_.shard_radius * 1.8f,
                      std::sin(angle) * radius};
    if (shard_overlaps_reserved_composition(shard.position)) {
      relocate_shard(shard, i);
    }
    shards_.push_back(shard);
  }

  for (int i = 0; i < tuning_.sentinel_count; ++i) {
    const float fill = (static_cast<float>(i) + 0.5f) / static_cast<float>(tuning_.sentinel_count);
    Sentinel sentinel;
    sentinel.phase = fill * kPi * 2.0f;
    sentinel.orbit_radius = tuning_.arena_radius * (0.38f + 0.34f * fill);
    sentinels_.push_back(sentinel);
  }

  rebuildScene();
  rebuildCaveWorldGate();
  clearTransientFeedback();
}

void LumenRun::update(const float dt, Vec2 move_axis, const bool run_requested,
                      const bool jump_requested) {
  ASTER_PROFILE_SCOPE("LumenRun::update");
  if (status_.victory || (status_.defeated && death_state_ == DeathSequenceState::Alive)) {
    return;
  }

  const float step = std::clamp(dt, 0.0f, 0.05f);
  const Vec3 previous_player_position = player_position_;
  if (forced_spawn_lighting_frames_ > 0) {
    --forced_spawn_lighting_frames_;
  }
  status_.elapsed_seconds += step;
  invulnerability_ = std::max(0.0f, invulnerability_ - step);
  updateBloodParticles(step);
  updateMiningFractureVisuals(step);

  if (death_state_ != DeathSequenceState::Alive) {
    updateDeathSequence(step);
    updateSceneObjects(step);
    advanceWorldProof(step, {}, false, false, previous_player_position);
    return;
  }

  if (length(move_axis) > 1.0f) {
    move_axis = normalize(move_axis);
  }
  if (player_avatar_point_enabled_ &&
      (length(move_axis) > 0.001f || run_requested || jump_requested)) {
    clearAvatarPointTarget();
  }

  updatePlayerPhysics(step, move_axis, run_requested, jump_requested);
  enforceWorldBounds();
  updateChestInteractionState();
  updatePrismRelay(step);
  updateCrocodile(step);
  updateCaveSkitters(step);
  updateClassicGauntlet(step);
  updateCastleBirds(step);

  collectOverlaps();
  resolveSentinelImpacts();
  updateSceneObjects(step);
  advanceWorldProof(step, move_axis, run_requested, jump_requested, previous_player_position);
}

const Scene &LumenRun::scene() const {
  return scene_;
}

const SceneCoherenceReport &LumenRun::sceneCoherenceReport() const {
  if (scene_coherence_dirty_) {
    rebuildSceneCoherenceReport();
  }
  return scene_coherence_;
}

const SceneTraceValidationReport &LumenRun::sceneTraceReport() const {
  if (scene_trace_dirty_) {
    rebuildSceneTraceReport();
  }
  return scene_trace_;
}

const LumenStatus &LumenRun::status() const {
  return status_;
}

const LumenWorldForensics &LumenRun::worldForensics() const {
  return world_forensics_;
}

const LumenCaveWorldGateReport &LumenRun::caveWorldGateReport() const {
  return world_forensics_.cave_gate;
}

bool LumenRun::caveWorldGateAccepted() const {
  return world_forensics_.cave_gate.verdict == LumenWorldGateVerdict::Accepted;
}

void LumenRun::noteRenderExtraction(const std::uint64_t extraction_hash,
                                    const std::uint64_t frame_submission_hash) {
  world_state_.noteRenderableExtraction(extraction_hash, frame_submission_hash);
  world_forensics_.render_extraction_hash = extraction_hash;
  world_forensics_.frame_submission_hash = frame_submission_hash;
  world_forensics_.render_extraction_ready = extraction_hash != 0u && frame_submission_hash != 0u;
  world_forensics_.world_hash = world_state_.worldHash();
  world_forensics_.trace_hash = world_state_.traceHash();
}

void LumenRun::resetWorldProof() {
  world_state_ = WorldState({.fixed_step_seconds = 1.0 / 60.0,
                             .seed = static_cast<std::uint64_t>(kLumenCaveSeed),
                             .label = "lumen.run.world"});
  perceptual_runtime_.setOptions(perceptualRuntimeOptions(0u));
  perceptual_runtime_.reset();
  world_forensics_ = {};
  next_world_epoch_ = 1u;
  world_forensics_.world_hash = world_state_.worldHash();
  world_forensics_.trace_hash = world_state_.traceHash();
}

PerceptualWorldRuntimeOptions
LumenRun::perceptualRuntimeOptions(const std::uint64_t region_id) const {
  PerceptualWorldRuntimeOptions options;
  options.region_id = region_id;
  options.id = "lumen_entry_perceptual_runtime";
  if (authoring_.valid && authoring_.cave.validation.perceptual_runtime.has_value()) {
    const sdk::CavePerceptualRuntimeDocument &runtime =
        *authoring_.cave.validation.perceptual_runtime;
    options.id = runtime.id.empty() ? options.id : runtime.id;
    options.exposure_horizon_seconds = runtime.exposure_horizon_seconds;
    options.minimum_continuity_score = runtime.minimum_continuity_score;
    options.minimum_occlusion_trust = runtime.minimum_occlusion_trust;
    options.minimum_lighting_believability = runtime.minimum_lighting_believability;
    options.minimum_player_readable_cause = runtime.minimum_player_readable_cause;
  }
  return options;
}

WorldPerceptionLedgerReport LumenRun::buildPerceptionLedgerReport(
    const std::uint64_t region_id) const {
  std::uint32_t ledger_required = lumenPerceptionLedgerDefaultMask();
  float ledger_minimum = 0.72f;
  std::vector<sdk::CavePerceptionLedgerCellDocument> authored_cells;
  if (authoring_.valid && authoring_.cave.validation.perception_ledger.has_value()) {
    const sdk::CavePerceptionLedgerDocument &ledger =
        *authoring_.cave.validation.perception_ledger;
    ledger_required = lumenPerceptionLedgerChannelMask(ledger.required_channels);
    ledger_minimum = ledger.minimum_score;
    authored_cells = ledger.cells;
  }

  std::vector<std::string> cell_ids;
  if (!authored_cells.empty()) {
    cell_ids.reserve(authored_cells.size());
    for (const sdk::CavePerceptionLedgerCellDocument &cell : authored_cells) {
      cell_ids.push_back(cell.id);
    }
  } else if (!cave_sections_.empty()) {
    cell_ids.reserve(cave_sections_.size());
    for (std::size_t index = 0u; index < cave_sections_.size(); ++index) {
      cell_ids.push_back("cave-section-" + std::to_string(index));
    }
  }
  if (cell_ids.empty()) {
    cell_ids.push_back("lumen-runtime");
  }

  std::size_t fixture_count = 0u;
  for (const AuthoredCaveSection &section : cave_sections_) {
    fixture_count += section.wall_fixtures.size() + section.secondary_wall_fixtures.size();
  }
  std::size_t live_ores = 0u;
  std::size_t damaged_ores = 0u;
  std::uint64_t material_hash = lumenHashString("ledger.material-memory", region_id);
  std::uint64_t wear_hash = lumenHashString("ledger.wear-continuity", region_id);
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      ++live_ores;
      material_hash = lumenHash(ore.position, material_hash);
      material_hash =
          lumenHash(material_hash, static_cast<std::uint64_t>(std::max(ore.health, 0)));
    }
    if (ore.health < ore.max_health || ore.collected) {
      ++damaged_ores;
    }
    wear_hash = lumenHash(wear_hash, static_cast<std::uint64_t>(std::max(ore.health, 0)));
    wear_hash = lumenHash(wear_hash, static_cast<std::uint64_t>(ore.collected ? 1u : 0u));
  }
  material_hash = lumenHash(material_hash, world_forensics_.coal_mining_reaction.material_memory_hash);
  wear_hash = lumenHash(wear_hash, world_forensics_.coal_mining_reaction.event_residue_hash);

  std::size_t alive_skitters = 0u;
  for (const CaveSkitter &skitter : cave_skitters_) {
    if (!skitter.dead && !skitter.state.dead) {
      ++alive_skitters;
    }
  }

  std::uint64_t contact_hash = lumenHashString("ledger.contact-history", region_id);
  contact_hash =
      lumenHash(contact_hash, static_cast<std::uint64_t>(cave_collision_meshes_.size()));
  contact_hash =
      lumenHash(contact_hash, static_cast<std::uint64_t>(scenery_collision_boxes_.size()));
  contact_hash = lumenHash(contact_hash, world_forensics_.coal_mining_reaction.event_residue_hash);

  std::uint64_t light_hash = lumenHashString("ledger.lighting-exposure", region_id);
  light_hash = lumenHash(light_hash, static_cast<std::uint64_t>(fixture_count));
  light_hash = lumenHash(cave_entrance_light_position_, light_hash);
  light_hash = lumenHash(light_hash, world_forensics_.coal_mining_reaction.lighting_atmosphere_hash);

  const CaveLightingState player_light = caveLightingStateAt(player_position_);
  std::uint64_t atmosphere_hash = lumenHashString("ledger.atmosphere-cell", region_id);
  atmosphere_hash = lumenHash(player_light.interior, atmosphere_hash);
  atmosphere_hash = lumenHash(player_light.depth, atmosphere_hash);
  atmosphere_hash = lumenHash(player_light.wall_light, atmosphere_hash);
  atmosphere_hash = lumenHash(atmosphere_hash, static_cast<std::uint64_t>(cave_sections_.size()));

  std::uint64_t occlusion_hash = lumenHashString("ledger.occlusion-role", region_id);
  occlusion_hash =
      lumenHash(occlusion_hash, static_cast<std::uint64_t>(cave_exterior_hidden_objects_.size()));
  occlusion_hash = lumenHash(occlusion_hash, static_cast<std::uint64_t>(scene_.objects().size()));

  std::uint64_t affordance_hash = lumenHashString("ledger.gameplay-affordance", region_id);
  affordance_hash = lumenHash(affordance_hash, static_cast<std::uint64_t>(live_ores));
  affordance_hash = lumenHash(affordance_hash, static_cast<std::uint64_t>(alive_skitters));
  affordance_hash = lumenHash(affordance_hash,
                              world_forensics_.coal_mining_reaction.reaction_package_hash);

  std::uint64_t streaming_hash = lumenHashString("ledger.streaming-semantic-lod", region_id);
  streaming_hash = lumenHash(streaming_hash, region_id);
  streaming_hash = lumenHash(streaming_hash, static_cast<std::uint64_t>(scene_.objects().size()));
  streaming_hash =
      lumenHash(streaming_hash, static_cast<std::uint64_t>(cave_sections_.size()));

  std::uint64_t cue_hash = lumenHashString("ledger.audio-visual-cue-budget", region_id);
  cue_hash = lumenHash(cue_hash, static_cast<std::uint64_t>(fixture_count));
  cue_hash = lumenHash(cue_hash, static_cast<std::uint64_t>(alive_skitters));
  cue_hash = lumenHash(cue_hash, static_cast<std::uint64_t>(live_ores));
  cue_hash = lumenHash(cue_hash, world_forensics_.coal_mining_reaction.audio_visual_cue_budget_hash);

  std::vector<WorldPerceptionLedgerCellReport> cells;
  cells.reserve(cell_ids.size());
  for (std::size_t index = 0u; index < cell_ids.size(); ++index) {
    std::uint32_t cell_required = ledger_required;
    float cell_minimum = ledger_minimum;
    if (index < authored_cells.size()) {
      const sdk::CavePerceptionLedgerCellDocument &cell = authored_cells[index];
      if (!cell.required_channels.empty()) {
        cell_required = lumenPerceptionLedgerChannelMask(cell.required_channels);
      }
      if (cell.minimum_score > 0.0f) {
        cell_minimum = cell.minimum_score;
      }
    }

    const std::uint64_t cell_hash = lumenHashString(cell_ids[index], region_id);
    WorldPerceptionLedgerCellDesc desc;
    desc.region_id = region_id;
    desc.cell_id = cell_ids[index];
    desc.required_channel_mask = cell_required;
    desc.minimum_score = cell_minimum;
    desc.diagnostic = "lumen cave authored sensory cell";
    if (live_ores > 0u || material_hash != 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::MaterialMemory,
                               lumenHash(material_hash, cell_hash), 1.0f});
    }
    if (!cave_collision_meshes_.empty() || !scenery_collision_boxes_.empty() ||
        world_forensics_.coal_mining_reaction.event_residue_hash != 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::ContactHistory,
                               lumenHash(contact_hash, cell_hash), 1.0f});
    }
    if (fixture_count > 0u || length(cave_entrance_light_position_) > 0.0f) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::LightingExposure,
                               lumenHash(light_hash, cell_hash), 1.0f});
    }
    if (!cave_sections_.empty() || player_light.interior > 0.0f) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::AtmosphereCell,
                               lumenHash(atmosphere_hash, cell_hash), 1.0f});
    }
    if (!scene_.objects().empty()) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::OcclusionRole,
                               lumenHash(occlusion_hash, cell_hash), 1.0f});
    }
    if (live_ores > 0u || alive_skitters > 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::GameplayAffordance,
                               lumenHash(affordance_hash, cell_hash), 1.0f});
    }
    if (live_ores > 0u || damaged_ores > 0u ||
        world_forensics_.coal_mining_reaction.event_residue_hash != 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::WearContinuity,
                               lumenHash(wear_hash, cell_hash), 1.0f});
    }
    if (region_id != 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::StreamingSemanticLod,
                               lumenHash(streaming_hash, cell_hash), 1.0f});
    }
    if (fixture_count > 0u || alive_skitters > 0u || live_ores > 0u) {
      desc.evidence.push_back({WorldPerceptionLedgerChannel::AudioVisualCueBudget,
                               lumenHash(cue_hash, cell_hash), 1.0f});
    }
    cells.push_back(evaluateWorldPerceptionLedgerCell(desc));
  }

  return summarizeWorldPerceptionLedger(region_id, ledger_required, ledger_minimum,
                                        std::move(cells));
}

std::vector<WorldPerceptionObjectTrace>
LumenRun::buildPerceptionObjectTraces(const WorldPerceptionLedgerReport &ledger) const {
  std::vector<WorldPerceptionObjectTrace> traces;
  if (ledger.cells.empty()) {
    return traces;
  }
  traces.reserve(scene_.objects().size());
  for (std::size_t index = 0u; index < scene_.objects().size(); ++index) {
    const RenderObject &object = scene_.objects()[index];
    const WorldPerceptionLedgerCellReport &cell = ledger.cells[index % ledger.cells.size()];
    std::uint64_t object_hash = lumenHashString(object.name, ledger.ledger_hash);
    object_hash = lumenHashString(object.material_asset_id, object_hash);
    object_hash = lumenHash(object.transform.position, object_hash);
    traces.push_back({.object_name = object.name,
                      .cell_id = cell.cell_id,
                      .observed_channel_mask = cell.observed_channel_mask,
                      .object_hash = object_hash,
                      .ledger_hash = cell.ledger_hash});
  }
  return traces;
}

PerceptualWorldObservation
LumenRun::makePerceptualObservation(const float dt, const Vec2 move_axis,
                                    const Vec3 previous_player_position) const {
  PerceptualWorldObservation observation =
      makePerceptualWorldObservation(world_forensics_.perception_ledger);
  observation.delta_seconds = dt;
  observation.world_transition_hash = world_forensics_.world_transition_hash;
  observation.visibility_set_hash = world_forensics_.visibility_set_hash;
  observation.region_id = world_forensics_.streaming_region_id != 0u
                              ? world_forensics_.streaming_region_id
                              : world_forensics_.cave_gate.region_id;
  observation.player_position = player_position_;
  observation.navigation_valid = world_forensics_.cave_gate.navigation_valid;
  observation.perceptual_salience_score =
      std::max(world_forensics_.cave_gate.perceptual_salience_score,
               world_forensics_.perception_ledger.score);
  observation.traversal_speed =
      dt > 0.0f ? length(player_position_ - previous_player_position) / std::max(dt, 0.001f)
                : length(move_axis);

  std::size_t live_ores = 0u;
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      ++live_ores;
    }
  }
  std::size_t alive_skitters = 0u;
  for (const CaveSkitter &skitter : cave_skitters_) {
    if (!skitter.dead && !skitter.state.dead) {
      ++alive_skitters;
    }
  }
  observation.resource_pressure = std::clamp(static_cast<float>(live_ores) / 8.0f, 0.0f, 1.0f);
  observation.encounter_pressure =
      std::clamp(static_cast<float>(alive_skitters) /
                     static_cast<float>(std::max(kCaveSkitterEncounterCount, 1)),
                 0.0f, 1.0f);
  const FocusPromptModel prompt = focusPromptModel();
  observation.explicit_player_readable_cause =
      (prompt.visible ? 0.45f : 0.0f) +
      (world_forensics_.coal_mining_reaction.accepted ? 0.55f : 0.0f);
  observation.reaction_package_hash =
      world_forensics_.coal_mining_reaction.reaction_package_hash;
  observation.material_memory_hash = world_forensics_.coal_mining_reaction.material_memory_hash;
  observation.contact_history_hash = world_forensics_.coal_mining_reaction.contact_history_hash;
  observation.lighting_atmosphere_hash =
      world_forensics_.coal_mining_reaction.lighting_atmosphere_hash;
  observation.wear_continuity_hash = world_forensics_.coal_mining_reaction.wear_continuity_hash;
  observation.ai_attention_hash = world_forensics_.coal_mining_reaction.ai_attention_hash;
  observation.streaming_residency_lod_hash =
      world_forensics_.coal_mining_reaction.streaming_residency_lod_hash;
  observation.resource_state_hash = world_forensics_.coal_mining_reaction.resource_state_hash;
  observation.event_residue_hash = world_forensics_.coal_mining_reaction.event_residue_hash;
  observation.audio_visual_cue_budget_hash =
      world_forensics_.coal_mining_reaction.audio_visual_cue_budget_hash;
  observation.readability_audit_hash =
      world_forensics_.coal_mining_reaction.readability_audit_hash;
  return observation;
}

BeliefExtractionReport LumenRun::buildBeliefExtractionReport() const {
  BeliefExtractionDesc desc;
  desc.subject = "lumen_run.cave_entry";
  desc.world_transition_hash = world_forensics_.world_transition_hash;
  desc.extraction_hash = world_forensics_.render_extraction_hash;
  desc.source_hash = world_forensics_.cave_gate.probe_trace_hash;
  desc.perception_ledger = world_forensics_.perception_ledger;
  desc.perceptual_state = world_forensics_.perceptual_state;
  if (authoring_.valid && authoring_.cave.validation.belief_contract.has_value()) {
    desc.minimum_score = authoring_.cave.validation.belief_contract->minimum_score;
  }

  desc.visible_object_count = scene_.objects().size();
  std::vector<std::string> material_families;
  material_families.reserve(scene_.objects().size());
  for (const RenderObject &object : scene_.objects()) {
    std::string family = object.material_asset_id;
    if (family.empty()) {
      family = std::string(materialSurfaceProfileName(resolveMaterialSurfaceProfile(object.material)));
    }
    if (family.empty()) {
      family = "runtime-material";
    }
    if (std::find(material_families.begin(), material_families.end(), family) ==
        material_families.end()) {
      material_families.push_back(std::move(family));
    }
  }
  desc.material_family_count = material_families.size();

  const bool has_contact = world_forensics_.perception_ledger.contact_history_hash != 0u;
  const bool has_lighting = world_forensics_.perception_ledger.lighting_exposure_hash != 0u;
  const bool has_atmosphere = world_forensics_.perception_ledger.atmosphere_cell_hash != 0u;
  const bool has_material_memory = world_forensics_.perception_ledger.material_memory_hash != 0u;
  const bool has_wear = world_forensics_.perception_ledger.wear_continuity_hash != 0u;
  const bool has_affordance =
      world_forensics_.perception_ledger.gameplay_affordance_hash != 0u;

  desc.contact_shadow_required = true;
  desc.contact_shadow_enabled = has_contact;
  desc.contact_shadow_credibility = has_contact ? 0.82f : 0.0f;
  desc.volumetric_required = has_atmosphere || has_lighting;
  desc.volumetric_scene_coupled = has_atmosphere && has_lighting;
  desc.volumetric_scene_coupling = desc.volumetric_scene_coupled ? 0.76f : 0.0f;
  desc.material_response_stability = has_material_memory && has_wear ? 0.80f : 0.40f;
  desc.lod_transition_invisibility =
      world_forensics_.perception_ledger.streaming_semantic_lod_hash != 0u ? 0.84f : 0.25f;
  desc.asset_scale_coherence = world_forensics_.cave_gate.navigation_valid ? 0.82f : 0.25f;
  desc.environmental_entropy = has_material_memory && has_wear && has_affordance ? 0.78f : 0.35f;
  return extractBeliefContract(desc);
}

void LumenRun::advancePerceptualRuntime(const float dt, const Vec2 move_axis,
                                        const Vec3 previous_player_position) {
  const std::uint64_t region_id = world_forensics_.streaming_region_id != 0u
                                      ? world_forensics_.streaming_region_id
                                      : world_forensics_.cave_gate.region_id;
  const PerceptualWorldRuntimeOptions options = perceptualRuntimeOptions(region_id);
  if (perceptual_runtime_.options().id != options.id ||
      perceptual_runtime_.options().region_id != options.region_id) {
    perceptual_runtime_.setOptions(options);
  }
  world_forensics_.perceptual_state =
      perceptual_runtime_.advance(makePerceptualObservation(dt, move_axis, previous_player_position));
}

void LumenRun::recordCoalMiningReaction(const std::size_t ore_index,
                                        const MiningFeedback &feedback,
                                        const CoalOreNode &ore) {
  LumenReactionPackageReport report;
  report.required_channel_mask = kContinuityMaterialMemory | kContinuityEventResidue |
                                 kContinuitySensoryFeedback | kContinuityResourceState |
                                 kContinuityAiAttention | kContinuityUiFeedback;
  report.minimum_score = 0.68f;
  if (authoring_.valid && authoring_.cave.validation.perceptual_continuity_budget.has_value()) {
    const sdk::CavePerceptualContinuityBudgetDocument &budget =
        *authoring_.cave.validation.perceptual_continuity_budget;
    for (const sdk::CaveReactionPackageDocument &package : budget.reaction_packages) {
      if (package.id == "coal_mining_reaction") {
        report.required_channel_mask = lumenContinuityChannelMask(package.required_channels);
        report.minimum_score = package.minimum_score;
        break;
      }
    }
  }

  std::uint64_t material_hash = lumenHashString("coal.material-memory", kLumenWorldSeed64);
  material_hash = lumenHash(material_hash, static_cast<std::uint64_t>(ore_index));
  material_hash = lumenHash(feedback.crack_fraction, material_hash);
  material_hash = lumenHash(material_hash, static_cast<std::uint64_t>(std::max(ore.health, 0)));
  material_hash = lumenHash(material_hash, static_cast<std::uint64_t>(std::max(ore.max_health, 1)));
  material_hash = lumenHash(feedback.impact_point, material_hash);
  report.material_memory_hash = material_hash;

  std::uint64_t residue_hash = lumenHashString("coal.event-residue", material_hash);
  for (const VoxelImpactEvent &event : feedback.impact_events) {
    residue_hash = lumenHash(residue_hash, static_cast<std::uint64_t>(event.kind));
    residue_hash = lumenHash(event.intensity, residue_hash);
    residue_hash = lumenHash(event.crack_fraction, residue_hash);
    residue_hash = lumenHash(residue_hash, static_cast<std::uint64_t>(std::max(event.particle_count, 0)));
  }
  report.event_residue_hash = residue_hash;
  std::uint64_t contact_hash = lumenHashString("coal.contact-history", residue_hash);
  contact_hash = lumenHash(contact_hash, static_cast<std::uint64_t>(feedback.impact_events.size()));
  contact_hash = lumenHash(feedback.impact_point, contact_hash);
  report.contact_history_hash = contact_hash;

  std::size_t attentive_skitters = 0u;
  std::uint64_t ai_hash = lumenHashString("coal.noise-attention", residue_hash);
  for (const CaveSkitter &skitter : cave_skitters_) {
    if (skitter.dead || skitter.state.dead) {
      continue;
    }
    const float distance_to_impact = length(skitter.state.position - feedback.impact_point);
    if (distance_to_impact <= 24.0f) {
      ++attentive_skitters;
      ai_hash = lumenHash(skitter.state.position, ai_hash);
      ai_hash = lumenHash(distance_to_impact, ai_hash);
    }
  }
  ai_hash = lumenHash(ai_hash, static_cast<std::uint64_t>(attentive_skitters));
  report.ai_attention_hash = ai_hash;

  std::uint64_t resource_hash = lumenHashString("coal.resource-state", material_hash);
  resource_hash = lumenHash(resource_hash, static_cast<std::uint64_t>(std::max(ore.health, 0)));
  resource_hash = lumenHash(resource_hash, static_cast<std::uint64_t>(ore.collected ? 1u : 0u));
  resource_hash =
      lumenHash(resource_hash, static_cast<std::uint64_t>(std::max(feedback.resource_quantity, 0)));
  report.resource_state_hash = resource_hash;
  std::uint64_t wear_hash = lumenHashString("coal.wear-continuity", resource_hash);
  wear_hash = lumenHash(wear_hash, static_cast<std::uint64_t>(std::max(ore.health, 0)));
  wear_hash = lumenHash(wear_hash, static_cast<std::uint64_t>(ore.collected ? 1u : 0u));
  wear_hash = lumenHash(feedback.crack_fraction, wear_hash);
  report.wear_continuity_hash = wear_hash;

  const InteractionFocus &focus = interaction_.focus();
  std::uint64_t readability_hash = lumenHashString("coal.ui-readability", resource_hash);
  readability_hash = lumenHash(focus.visible, readability_hash);
  readability_hash = lumenHash(readability_hash, static_cast<std::uint64_t>(focus.user_data));
  readability_hash = lumenHashString(focus.subject_label, readability_hash);
  report.readability_audit_hash = readability_hash;
  report.lighting_atmosphere_hash = world_forensics_.cave_gate.perceptual_continuity_report_hash;
  report.streaming_residency_lod_hash = world_forensics_.streaming_region_id;
  std::uint64_t cue_hash = lumenHashString("coal.audio-visual-cue-budget", readability_hash);
  cue_hash = lumenHash(cue_hash, report.event_residue_hash);
  cue_hash = lumenHash(cue_hash, report.ai_attention_hash);
  cue_hash = lumenHash(cue_hash, report.contact_history_hash);
  report.audio_visual_cue_budget_hash = cue_hash;

  report.observed_channel_mask = 0u;
  if (report.material_memory_hash != 0u) {
    report.observed_channel_mask |= kContinuityMaterialMemory;
  }
  if (report.event_residue_hash != 0u) {
    report.observed_channel_mask |= kContinuityEventResidue | kContinuitySensoryFeedback;
  }
  if (report.ai_attention_hash != 0u) {
    report.observed_channel_mask |= kContinuityAiAttention;
  }
  if (report.resource_state_hash != 0u) {
    report.observed_channel_mask |= kContinuityResourceState;
  }
  if (report.readability_audit_hash != 0u) {
    report.observed_channel_mask |= kContinuityUiFeedback;
  }
  report.missing_channel_mask = report.required_channel_mask & ~report.observed_channel_mask;
  report.continuity_score =
      lumenContinuityScore(report.required_channel_mask, report.observed_channel_mask);
  report.accepted =
      report.missing_channel_mask == 0u && report.continuity_score + 0.0001f >= report.minimum_score;
  std::uint64_t package_hash = lumenHashString("coal.reaction-package", material_hash);
  package_hash = lumenHash(package_hash, report.material_memory_hash);
  package_hash = lumenHash(package_hash, report.contact_history_hash);
  package_hash = lumenHash(package_hash, report.event_residue_hash);
  package_hash = lumenHash(package_hash, report.ai_attention_hash);
  package_hash = lumenHash(package_hash, report.resource_state_hash);
  package_hash = lumenHash(package_hash, report.wear_continuity_hash);
  package_hash = lumenHash(package_hash, report.audio_visual_cue_budget_hash);
  package_hash = lumenHash(package_hash, report.readability_audit_hash);
  package_hash = lumenHash(report.continuity_score, package_hash);
  report.reaction_package_hash = package_hash;
  report.diagnostic = report.accepted ? "coal mining reaction continuity accepted"
                                      : "coal mining reaction continuity missing channels";
  world_forensics_.coal_mining_reaction = std::move(report);
  world_forensics_.perception_ledger =
      buildPerceptionLedgerReport(world_forensics_.streaming_region_id);
  world_forensics_.perception_object_traces =
      buildPerceptionObjectTraces(world_forensics_.perception_ledger);
}

void LumenRun::rebuildCaveWorldGate() {
  LumenCaveWorldGateReport report;
  report.seed = authoring_.valid && authoring_.cave.validation.probe_agent.has_value()
                    ? authoring_.cave.validation.probe_agent->seed
                    : static_cast<std::uint64_t>(kLumenCaveSeed);
  report.region_id = lumenHash(kLumenEntryRegionId, report.seed);
  report.perceptual_minimum_salience =
      authoring_.valid && authoring_.cave.validation.perceptual_budget.has_value()
          ? authoring_.cave.validation.perceptual_budget->minimum_salience
          : 0.50f;

  std::uint64_t nav_hash = lumenHashString("lumen.cave.nav", report.seed);
  const float route_step =
      authoring_.valid && authoring_.cave.validation.probe_agent.has_value()
          ? std::max(authoring_.cave.validation.probe_agent->step_length, 0.25f)
          : 1.35f;
  const auto routePointHasSupport = [&](const Vec3 point, const float tolerance) {
    const float support_tolerance = std::max(tolerance, 0.10f);
    const TerrainSurfaceSample support =
        sampleWorldSupport({{point.x, point.z}, point.y + 0.38f, 0.72f,
                            std::max(4.80f, support_tolerance + 1.20f)});
    return support.valid && std::abs(support.height - point.y) <= support_tolerance + 0.18f;
  };
  const auto probeRoute = [&](const std::vector<Vec3> &points, const float max_segment_length,
                              const float support_tolerance) {
    if (points.size() < 2u) {
      report.blocked_steps += 1u;
      nav_hash = lumenHashString("route-missing", nav_hash);
      return;
    }
    nav_hash = lumenHash(lumenRouteLength(points), nav_hash);
    for (std::size_t i = 1u; i < points.size(); ++i) {
      const Vec3 from = points[i - 1u];
      const Vec3 to = points[i];
      const float segment_length = length(to - from);
      nav_hash = lumenHash(from, nav_hash);
      nav_hash = lumenHash(to, nav_hash);
      const bool length_ok = segment_length <= max_segment_length + support_tolerance;
      const int steps =
          std::max(1, static_cast<int>(std::ceil(segment_length / std::max(route_step, 0.25f))));
      if (!length_ok) {
        report.blocked_steps += 1u;
        nav_hash = lumenHashString("segment-too-long", nav_hash);
      }
      for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const Vec3 point = from + (to - from) * t;
        ++report.checked_steps;
        if (!routePointHasSupport(point, support_tolerance)) {
          ++report.blocked_steps;
          nav_hash = lumenHash(point, nav_hash);
        }
      }
    }
  };

  const auto probeBuiltCaveCenterlines = [&]() {
    for (const AuthoredCaveSection &section : cave_sections_) {
      std::vector<Vec3> points;
      points.reserve(13u);
      for (int step = 0; step <= 12; ++step) {
        const float t = static_cast<float>(step) / 12.0f;
        points.push_back(sampleCaveTunnelFrame(section.tunnel, t).floor_center);
      }
      probeRoute(points, 7.25f, 0.65f);
    }
  };

  if (!cave_sections_.empty()) {
    probeBuiltCaveCenterlines();
  } else if (authoring_.valid && !authoring_.cave.validation.walkable_routes.empty()) {
    for (const sdk::CaveRouteValidationDocument &route :
         authoring_.cave.validation.walkable_routes) {
      std::vector<Vec3> points;
      points.reserve(route.points.size());
      for (const sdk::Vec3 &point : route.points) {
        points.push_back(lumenSdkVec(point));
      }
      nav_hash = lumenHashString(route.id, nav_hash);
      probeRoute(points, route.max_segment_length, route.support_tolerance);
    }
  }
  if (authoring_.valid && !authoring_.cave.validation.walkable_routes.empty()) {
    for (const sdk::CaveRouteValidationDocument &route :
         authoring_.cave.validation.walkable_routes) {
      std::vector<Vec3> points;
      points.reserve(route.points.size());
      for (const sdk::Vec3 &point : route.points) {
        points.push_back(lumenSdkVec(point));
      }
      nav_hash = lumenHashString(route.id, nav_hash);
      nav_hash = lumenHash(lumenRouteLength(points), nav_hash);
      if (lumenRouteLength(points) > route.max_segment_length + route.support_tolerance) {
        report.blocked_steps += 1u;
        nav_hash = lumenHashString("authored-anchor-span-too-long", nav_hash);
      }
    }
  }
  report.navigation_valid = report.checked_steps > 0u && report.blocked_steps == 0u;
  report.nav_report_hash = nav_hash;

  std::uint64_t resource_hash = lumenHashString("lumen.cave.resources", report.seed);
  const auto countResourcesNear = [&](const Vec3 position, const float radius) {
    std::size_t count = 0u;
    const float radius_sq = radius * radius;
    for (const CoalOreNode &ore : coal_ores_) {
      if (ore.collected) {
        continue;
      }
      const Vec3 delta = ore.position - position;
      if (dot(delta, delta) <= radius_sq) {
        ++count;
        resource_hash = lumenHash(ore.position, resource_hash);
      }
    }
    return count;
  };
  if (authoring_.valid && !authoring_.cave.validation.resource_probes.empty()) {
    for (const sdk::CaveWorldProbeDocument &probe : authoring_.cave.validation.resource_probes) {
      const Vec3 position = lumenSdkVec(probe.position);
      const std::size_t count = countResourcesNear(position, std::max(probe.radius, 0.01f));
      report.reachable_resources += count;
      report.required_resources +=
          static_cast<std::size_t>(std::max(probe.minimum_count, 0));
      resource_hash = lumenHashString(probe.id, resource_hash);
      resource_hash = lumenHash(position, resource_hash);
      resource_hash = lumenHash(resource_hash, static_cast<std::uint64_t>(count));
    }
  } else {
    report.reachable_resources = countResourcesNear(kCaveEntrancePlanar, 80.0f);
    report.required_resources = coal_ores_.empty() ? 0u : 1u;
  }
  report.resource_valid = report.reachable_resources >= report.required_resources;
  report.resource_probe_hash = resource_hash;

  std::uint64_t encounter_hash = lumenHashString("lumen.cave.encounters", report.seed);
  const auto countEncountersNear = [&](const Vec3 position, const float radius) {
    std::size_t count = 0u;
    const float radius_sq = radius * radius;
    for (const CaveSkitter &skitter : cave_skitters_) {
      if (skitter.dead || skitter.state.dead) {
        continue;
      }
      const Vec3 delta = skitter.state.position - position;
      if (dot(delta, delta) <= radius_sq) {
        ++count;
        encounter_hash = lumenHash(skitter.state.position, encounter_hash);
      }
    }
    return count;
  };
  if (authoring_.valid && !authoring_.cave.validation.encounter_probes.empty()) {
    float minimum_budget = 0.0f;
    float maximum_budget = 0.0f;
    for (const sdk::CaveWorldProbeDocument &probe : authoring_.cave.validation.encounter_probes) {
      const Vec3 position = lumenSdkVec(probe.position);
      const std::size_t count = countEncountersNear(position, std::max(probe.radius, 0.01f));
      const float local_budget = std::min(static_cast<float>(count) * 0.25f,
                                          std::max(probe.maximum_budget, 0.0f));
      report.reachable_encounters += count;
      report.encounter_budget += local_budget;
      minimum_budget += std::max(probe.minimum_budget, 0.0f);
      maximum_budget += std::max(probe.maximum_budget, 0.0f);
      encounter_hash = lumenHashString(probe.id, encounter_hash);
      encounter_hash = lumenHash(position, encounter_hash);
      encounter_hash = lumenHash(local_budget, encounter_hash);
    }
    if (report.encounter_budget < minimum_budget && !cave_skitters_.empty()) {
      const std::size_t runtime_reachable =
          countEncountersNear(kCaveEntrancePlanar, tuning_.playable_radius);
      const float runtime_budget =
          std::min(static_cast<float>(runtime_reachable) * 0.25f, std::max(maximum_budget, 1.0f));
      report.reachable_encounters += runtime_reachable;
      report.encounter_budget += runtime_budget;
      encounter_hash = lumenHashString("runtime-skitter-affordance-fallback", encounter_hash);
      encounter_hash = lumenHash(encounter_hash, static_cast<std::uint64_t>(runtime_reachable));
      encounter_hash = lumenHash(runtime_budget, encounter_hash);
    }
    report.encounter_valid = report.encounter_budget >= minimum_budget;
  } else {
    report.reachable_encounters = countEncountersNear(kCaveEntrancePlanar, 90.0f);
    report.encounter_budget = static_cast<float>(report.reachable_encounters) * 0.25f;
    report.encounter_valid = report.reachable_encounters > 0u;
  }
  report.encounter_budget_hash = encounter_hash;

  std::size_t fixture_count = 0u;
  for (const AuthoredCaveSection &section : cave_sections_) {
    fixture_count += section.wall_fixtures.size() + section.secondary_wall_fixtures.size();
  }
  const float resource_signal =
      std::min(static_cast<float>(report.reachable_resources) * 0.018f, 0.22f);
  const float encounter_signal =
      std::min(static_cast<float>(report.reachable_encounters) * 0.075f, 0.24f);
  const float fixture_signal = std::min(static_cast<float>(fixture_count) * 0.012f, 0.24f);
  const float web_signal = std::min(static_cast<float>(cave_webs_.size()) * 0.080f, 0.14f);
  const float navigation_signal = report.navigation_valid ? 0.12f : 0.0f;
  report.perceptual_salience_score =
      std::clamp(0.20f + resource_signal + encounter_signal + fixture_signal + web_signal +
                     navigation_signal,
                 0.0f, 1.0f);
  report.perceptual_valid =
      report.perceptual_salience_score + 0.0001f >= report.perceptual_minimum_salience;
  std::uint64_t perceptual_hash = lumenHashString("lumen.cave.perceptual", report.seed);
  perceptual_hash = lumenHash(report.perceptual_salience_score, perceptual_hash);
  perceptual_hash = lumenHash(report.perceptual_minimum_salience, perceptual_hash);
  perceptual_hash = lumenHash(perceptual_hash, static_cast<std::uint64_t>(fixture_count));
  perceptual_hash = lumenHash(perceptual_hash, static_cast<std::uint64_t>(cave_webs_.size()));
  report.perceptual_report_hash = perceptual_hash;

  const WorldPerceptionLedgerReport perception_ledger =
      buildPerceptionLedgerReport(report.region_id);

  std::uint32_t continuity_required = 0u;
  std::uint32_t continuity_observed = 0u;
  float continuity_minimum = 0.0f;
  bool reaction_packages_valid = true;
  if (authoring_.valid && authoring_.cave.validation.perceptual_continuity_budget.has_value()) {
    const sdk::CavePerceptualContinuityBudgetDocument &budget =
        *authoring_.cave.validation.perceptual_continuity_budget;
    continuity_required = lumenContinuityChannelMask(budget.required_channels);
    continuity_minimum = budget.minimum_score;
    for (const sdk::CaveReactionPackageDocument &package : budget.reaction_packages) {
      const std::uint32_t package_required =
          lumenContinuityChannelMask(package.required_channels);
      const std::uint32_t package_observed =
          package.action == "action.mine.coal_ore"
              ? kContinuityMaterialMemory | kContinuityEventResidue |
                    kContinuitySensoryFeedback | kContinuityResourceState |
                    kContinuityAiAttention | kContinuityUiFeedback
              : 0u;
      continuity_observed |= package_observed;
      const float package_score = lumenContinuityScore(package_required, package_observed);
      reaction_packages_valid =
          reaction_packages_valid &&
          ((package_required & ~package_observed) == 0u &&
           package_score + 0.0001f >= package.minimum_score);
    }
  }
  if (report.navigation_valid && report.checked_steps > 0u) {
    continuity_observed |= kContinuitySpatialAffordance | kContinuityMotionContinuity |
                           kContinuityStreamingResidency;
  }
  if (report.reachable_encounters > 0u) {
    continuity_observed |= kContinuityHazardReadability | kContinuityAiAttention;
  }
  if (report.reachable_resources > 0u) {
    continuity_observed |= kContinuityMaterialMemory | kContinuityEventResidue |
                           kContinuityResourceState;
  }
  if (fixture_count > 0u) {
    continuity_observed |= kContinuityLightingAtmosphere;
  }
  if (perception_ledger.material_memory_hash != 0u) {
    continuity_observed |= kContinuityMaterialMemory | kContinuityResourceState;
  }
  if (perception_ledger.contact_history_hash != 0u ||
      perception_ledger.wear_continuity_hash != 0u) {
    continuity_observed |= kContinuityEventResidue;
  }
  if (perception_ledger.lighting_exposure_hash != 0u ||
      perception_ledger.atmosphere_cell_hash != 0u) {
    continuity_observed |= kContinuityLightingAtmosphere;
  }
  if (perception_ledger.gameplay_affordance_hash != 0u ||
      perception_ledger.occlusion_role_hash != 0u) {
    continuity_observed |= kContinuityHazardReadability | kContinuityAiAttention;
  }
  if (perception_ledger.streaming_semantic_lod_hash != 0u) {
    continuity_observed |= kContinuityStreamingResidency | kContinuityMotionContinuity;
  }
  if (perception_ledger.audio_visual_cue_budget_hash != 0u) {
    continuity_observed |= kContinuitySensoryFeedback;
  }
  report.perceptual_continuity_required_channel_mask = continuity_required;
  report.perceptual_continuity_observed_channel_mask = continuity_observed;
  report.perceptual_continuity_missing_channel_mask = continuity_required & ~continuity_observed;
  report.perceptual_continuity_score =
      lumenContinuityScore(continuity_required, continuity_observed);
  report.perceptual_continuity_minimum_score = continuity_minimum;
  report.perceptual_continuity_valid =
      (continuity_required == 0u ||
       (report.perceptual_continuity_missing_channel_mask == 0u &&
        report.perceptual_continuity_score + 0.0001f >= continuity_minimum)) &&
      reaction_packages_valid;
  std::uint64_t continuity_hash = lumenHashString("lumen.cave.continuity", report.seed);
  continuity_hash = lumenHash(continuity_hash, continuity_required);
  continuity_hash = lumenHash(continuity_hash, continuity_observed);
  continuity_hash = lumenHash(report.perceptual_continuity_score, continuity_hash);
  continuity_hash = lumenHash(continuity_hash, perception_ledger.ledger_hash);
  report.perceptual_continuity_report_hash = continuity_hash;
  report.perception_ledger_valid = perception_ledger.accepted;
  report.perception_ledger_required_channel_mask = perception_ledger.required_channel_mask;
  report.perception_ledger_observed_channel_mask = perception_ledger.observed_channel_mask;
  report.perception_ledger_missing_channel_mask = perception_ledger.missing_channel_mask;
  report.perception_ledger_score = perception_ledger.score;
  report.perception_ledger_minimum_score = perception_ledger.minimum_score;
  report.perception_ledger_cell_count = perception_ledger.cell_count;
  report.perception_ledger_hash = perception_ledger.ledger_hash;

  bool accepted = report.navigation_valid && report.resource_valid && report.encounter_valid &&
                  report.perceptual_valid && report.perceptual_continuity_valid &&
                  report.perception_ledger_valid;
  report.verdict =
      accepted ? LumenWorldGateVerdict::Accepted : LumenWorldGateVerdict::Quarantined;
  if (!report.navigation_valid) {
    report.diagnostic = appendGateReason(report.diagnostic, "navigation probe failed");
  }
  if (!report.resource_valid) {
    report.diagnostic = appendGateReason(report.diagnostic, "resource pressure probe failed");
  }
  if (!report.encounter_valid) {
    report.diagnostic = appendGateReason(report.diagnostic, "encounter budget probe failed");
  }
  if (!report.perceptual_valid) {
    report.diagnostic = appendGateReason(report.diagnostic, "perceptual salience below budget");
  }
  if (!report.perceptual_continuity_valid) {
    report.diagnostic =
        appendGateReason(report.diagnostic, "perceptual continuity below budget");
  }
  if (!report.perception_ledger_valid) {
    report.diagnostic =
        appendGateReason(report.diagnostic, "world perception ledger below budget");
  }
  if (report.diagnostic.empty()) {
    report.diagnostic = "runtime generated cave world gate accepted";
  }

  std::uint64_t probe_hash = lumenHashString("lumen.cave.world-gate", report.seed);
  probe_hash = lumenHash(probe_hash, report.region_id);
  probe_hash = lumenHash(probe_hash, report.nav_report_hash);
  probe_hash = lumenHash(probe_hash, report.resource_probe_hash);
  probe_hash = lumenHash(probe_hash, report.encounter_budget_hash);
  probe_hash = lumenHash(probe_hash, report.perceptual_report_hash);
  probe_hash = lumenHash(probe_hash, report.perceptual_continuity_report_hash);
  probe_hash = lumenHash(probe_hash, report.perception_ledger_hash);
  probe_hash = lumenHashString(lumenGateVerdictName(report.verdict), probe_hash);
  report.probe_trace_hash = probe_hash;

  world_state_.noteRegionGate(report.region_id, accepted, report.probe_trace_hash,
                              report.diagnostic);
  world_forensics_.cave_gate = report;
  world_forensics_.streaming_region_id = report.region_id;
  world_forensics_.world_hash = world_state_.worldHash();
  world_forensics_.trace_hash = world_state_.traceHash();
  world_forensics_.world_transition_hash = report.probe_trace_hash;
  world_forensics_.perception_ledger = perception_ledger;
  world_forensics_.perception_object_traces =
      buildPerceptionObjectTraces(world_forensics_.perception_ledger);
  world_forensics_.belief_report = buildBeliefExtractionReport();
  perceptual_runtime_.setOptions(perceptualRuntimeOptions(report.region_id));
  perceptual_runtime_.reset();
}

void LumenRun::advanceWorldProof(const float dt, const Vec2 move_axis, const bool run_requested,
                                 const bool jump_requested, const Vec3 previous_player_position) {
  const float step = dt > 0.0f ? dt : 1.0f / 60.0f;
  std::uint64_t input_hash = lumenHashString("input", kLumenWorldSeed64);
  input_hash = lumenHash(move_axis, input_hash);
  input_hash = lumenHash(run_requested, input_hash);
  input_hash = lumenHash(jump_requested, input_hash);

  std::uint64_t intent_hash = lumenHashString("player-intent", input_hash);
  intent_hash = lumenHash(player_position_, intent_hash);
  intent_hash = lumenHash(player_facing_yaw_, intent_hash);
  intent_hash = lumenHash(player_grounded_, intent_hash);

  std::size_t alive_skitters = 0u;
  for (const CaveSkitter &skitter : cave_skitters_) {
    if (!skitter.dead && !skitter.state.dead) {
      ++alive_skitters;
    }
  }
  std::size_t live_ores = 0u;
  for (const CoalOreNode &ore : coal_ores_) {
    if (!ore.collected) {
      ++live_ores;
    }
  }
  const std::uint64_t ledger_region =
      world_forensics_.streaming_region_id != 0u ? world_forensics_.streaming_region_id
                                                 : world_forensics_.cave_gate.region_id;
  world_forensics_.perception_ledger = buildPerceptionLedgerReport(ledger_region);
  world_forensics_.perception_object_traces =
      buildPerceptionObjectTraces(world_forensics_.perception_ledger);

  std::uint64_t actor_hash = lumenHashString("actor-delta", intent_hash);
  actor_hash = lumenHash(previous_player_position, actor_hash);
  actor_hash = lumenHash(player_position_, actor_hash);
  actor_hash = lumenHash(player_velocity_, actor_hash);
  actor_hash = lumenHash(actor_hash, static_cast<std::uint64_t>(status_.score));
  actor_hash = lumenHash(actor_hash, static_cast<std::uint64_t>(status_.health));
  actor_hash = lumenHash(actor_hash, static_cast<std::uint64_t>(alive_skitters));
  actor_hash = lumenHash(actor_hash, static_cast<std::uint64_t>(live_ores));
  actor_hash = lumenHash(actor_hash, world_forensics_.coal_mining_reaction.reaction_package_hash);
  actor_hash = lumenHash(actor_hash, world_forensics_.coal_mining_reaction.resource_state_hash);
  actor_hash = lumenHash(actor_hash, world_forensics_.perception_ledger.ledger_hash);

  const CaveLightingState cave_light = caveLightingStateAt(player_position_);
  const FocusPromptModel prompt = focusPromptModel();
  std::uint64_t sensory_hash = lumenHashString("sensory-event", actor_hash);
  sensory_hash = lumenHash(cave_light.interior, sensory_hash);
  sensory_hash = lumenHash(cave_light.depth, sensory_hash);
  sensory_hash = lumenHash(cave_light.wall_light, sensory_hash);
  sensory_hash = lumenHash(prompt.visible, sensory_hash);
  sensory_hash = lumenHashString(prompt.subject, sensory_hash);
  sensory_hash = lumenHash(sensory_hash, world_forensics_.coal_mining_reaction.event_residue_hash);
  sensory_hash = lumenHash(sensory_hash, world_forensics_.coal_mining_reaction.ai_attention_hash);
  sensory_hash = lumenHash(sensory_hash, world_forensics_.perception_ledger.ledger_hash);
  sensory_hash =
      lumenHash(sensory_hash, world_forensics_.perception_ledger.audio_visual_cue_budget_hash);

  std::uint64_t visibility_hash = lumenHashString("visibility-set", sensory_hash);
  visibility_hash = lumenHash(visibility_hash, static_cast<std::uint64_t>(scene_.objects().size()));
  visibility_hash = lumenHash(visibility_hash, world_forensics_.streaming_region_id);
  visibility_hash = lumenHash(cave_light.entrance_light, visibility_hash);
  visibility_hash = lumenHash(cave_light.chamber, visibility_hash);
  visibility_hash =
      lumenHash(visibility_hash, world_forensics_.perception_ledger.occlusion_role_hash);
  visibility_hash =
      lumenHash(visibility_hash, world_forensics_.perception_ledger.streaming_semantic_lod_hash);
  world_forensics_.actor_state_delta_hash = actor_hash;
  world_forensics_.actor_delta_count =
      1u + alive_skitters + (length(player_position_ - previous_player_position) > 0.0001f ? 1u
                                                                                            : 0u);
  world_forensics_.sensory_event_hash = sensory_hash;
  world_forensics_.visibility_set_hash = visibility_hash;
  advancePerceptualRuntime(step, move_axis, previous_player_position);
  world_forensics_.belief_report = buildBeliefExtractionReport();

  const WorldTickResult tick =
      world_state_.tick({.tick = next_world_epoch_++,
                         .delta_seconds = step,
                         .input_event_hash = input_hash,
                         .asset_lineage_hash = world_forensics_.cave_gate.probe_trace_hash});
  if (tick.accepted) {
    world_forensics_.epoch = tick.tick;
    world_forensics_.world_hash = tick.world_hash;
    world_forensics_.trace_hash = tick.trace_hash;
  } else {
    world_forensics_.world_hash = world_state_.worldHash();
    world_forensics_.trace_hash = world_state_.traceHash();
  }
  world_forensics_.actor_state_delta_hash = actor_hash;
  world_forensics_.actor_delta_count =
      1u + alive_skitters + (length(player_position_ - previous_player_position) > 0.0001f ? 1u
                                                                                            : 0u);
  world_forensics_.sensory_event_hash = sensory_hash;
  world_forensics_.visibility_set_hash = visibility_hash;

  std::uint64_t transition_hash = lumenHashString("world-transition", world_forensics_.world_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.trace_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.epoch);
  transition_hash = lumenHash(transition_hash, input_hash);
  transition_hash = lumenHash(transition_hash, intent_hash);
  transition_hash = lumenHash(transition_hash, actor_hash);
  transition_hash = lumenHash(transition_hash, sensory_hash);
  transition_hash = lumenHash(transition_hash, visibility_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.cave_gate.probe_trace_hash);
  transition_hash =
      lumenHash(transition_hash, world_forensics_.coal_mining_reaction.reaction_package_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.perception_ledger.ledger_hash);
  transition_hash =
      lumenHash(transition_hash, world_forensics_.perceptual_state.perceptual_state_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.perceptual_state.semantic_budget_hash);
  transition_hash = lumenHash(transition_hash, world_forensics_.belief_report.belief_contract_hash);
  world_forensics_.world_transition_hash = transition_hash;
}

Vec3 LumenRun::playerPosition() const {
  return player_position_;
}

Vec3 LumenRun::playerRenderPosition() const {
  return player_render_position_valid_ ? player_render_position_ : player_position_;
}

Vec3 LumenRun::prismRelayBasePosition() const {
  return prism_relay_base_;
}

Vec3 LumenRun::prismRelayFocusPosition() const {
  return prism_relay_base_ + Vec3{0.0f, kPrismRelayCoreHeight, 0.0f};
}

Vec3 LumenRun::caveFrameReportPosition(const float progress_distance) const {
  if (cave_sections_.empty()) {
    return player_position_;
  }
  const AuthoredCaveSection &section =
      cave_sections_.size() > 1u ? cave_sections_[1u] : cave_sections_.front();
  const CaveTunnelFrame frame =
      sampleCaveTunnelFrameAtDistance(section.tunnel, std::max(progress_distance, 0.0f));
  return frame.floor_center + frame.up * playerSupportExtent();
}

Vec3 LumenRun::caveFrameReportLookTarget(const float progress_distance,
                                         const float look_ahead) const {
  if (cave_sections_.empty()) {
    return player_position_ + Vec3{0.0f, 0.62f, 0.0f};
  }
  const AuthoredCaveSection &section =
      cave_sections_.size() > 1u ? cave_sections_[1u] : cave_sections_.front();
  const CaveTunnelFrame frame =
      sampleCaveTunnelFrameAtDistance(section.tunnel, std::max(progress_distance, 0.0f));
  const Vec3 tangent = length(frame.tangent) > 0.0001f ? normalize(frame.tangent)
                                                       : Vec3{0.0f, 0.0f, -1.0f};
  return frame.floor_center + frame.up * 0.88f + tangent * std::max(look_ahead, 0.0f);
}

float LumenRun::caveFrameReportCameraYaw(const float progress_distance) const {
  if (cave_sections_.empty()) {
    return 0.0f;
  }
  const AuthoredCaveSection &section =
      cave_sections_.size() > 1u ? cave_sections_[1u] : cave_sections_.front();
  const CaveTunnelFrame frame =
      sampleCaveTunnelFrameAtDistance(section.tunnel, std::max(progress_distance, 0.0f));
  const Vec3 tangent = length(frame.tangent) > 0.0001f ? normalize(frame.tangent)
                                                       : Vec3{0.0f, 0.0f, -1.0f};
  return std::atan2(-tangent.x, -tangent.z);
}

void LumenRun::relocatePlayer(Vec3 position, const float facing_yaw) {
  const TerrainSurfaceSample ground =
      sampleWorldSupport({{position.x, position.z}, position.y, 0.64f, 6.0f});
  if (ground.valid) {
    position.y = ground.height + playerSupportExtent();
  }
  player_position_ = position;
  player_velocity_ = {};
  player_facing_yaw_ = facing_yaw;
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  forced_spawn_lighting_frames_ = 0;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  updateSceneObjects(1.0f / 60.0f);
}

void LumenRun::updateRenderInterpolation(const float alpha) {
  if (death_state_ != DeathSequenceState::Alive || player_preview_yaw_enabled_ ||
      !player_avatar_pose_valid_ || !physics_.valid(player_body_) ||
      player_avatar_instance_.object_indices.empty()) {
    player_render_position_ = player_position_;
    player_render_position_valid_ = true;
    return;
  }

  const PhysicsBody &body = physics_.body(player_body_);
  player_render_position_ =
      body.previous_position + (body.position - body.previous_position) * clamp(alpha, 0.0f, 1.0f);
  player_render_position_valid_ = true;

  AvatarPose render_pose = player_avatar_pose_;
  render_pose.position = avatarPosePosition(player_render_position_);
  applyAvatarPose(scene_, player_avatar_, player_avatar_instance_, render_pose);
  if (!player_avatar_visible_) {
    for (const std::size_t object_index : player_avatar_instance_.object_indices) {
      if (object_index < scene_.objects().size()) {
        hideRenderObject(scene_.objects()[object_index]);
      }
    }
  }
}

float LumenRun::resolveCameraRadius(const Vec3 target, const float yaw, const float pitch,
                                    const float desired_radius) const {
  if (desired_radius <= 0.0f) {
    return desired_radius;
  }
  CaveInteriorSample cave_target{};
  const AuthoredCaveSection *cave_section = caveSectionAt(target, &cave_target);
  const bool cave_visibility_camera = cave_target.interior > 0.045f;
  const float minimum_radius =
      cave_visibility_camera
          ? std::max(0.82f, std::max(player_avatar_support_extent_, 0.0f) * 1.05f)
          : std::max(1.85f, std::max(player_avatar_support_extent_, 0.0f) * 1.80f);
  constexpr float collision_minimum_radius = 0.55f;
  constexpr float cast_radius = 0.18f;
  constexpr float hit_clearance = 0.32f;
  constexpr float surface_clearance = 0.42f;
  constexpr float search_step = 0.32f;
  const float cp = std::cos(pitch);
  const auto offset_for_radius = [&](const float radius) {
    return Vec3{radius * cp * std::sin(yaw), radius * std::sin(pitch), radius * cp * std::cos(yaw)};
  };
  const Vec3 desired_offset = offset_for_radius(desired_radius);
  PhysicsShapeCastHit hit;
  PhysicsSphereCast cast;
  cast.origin = target;
  cast.displacement = desired_offset;
  cast.radius = cast_radius;
  cast.filter.collides_with = kPhysicsLayerWorld;
  cast.filter.ignore_body = player_body_;
  float resolved_radius = desired_radius;
  float lower_bound = minimum_radius;
  if (physics_.castSphere(cast, hit)) {
    lower_bound = cave_visibility_camera ? minimum_radius : collision_minimum_radius;
    resolved_radius = std::clamp(hit.distance - hit_clearance, lower_bound, desired_radius);
  }
  if (cave_section != nullptr && cave_visibility_camera) {
    const CaveViewConstraint view_constraint =
        constrainCaveViewSegment(cave_section->tunnel, target,
                                 target + offset_for_radius(resolved_radius),
                                 {.samples = 36,
                                  .minimum_radius = minimum_radius,
                                  .interior_threshold = 0.045f,
                                  .backtrack_tolerance_t = 0.18f});
    if (view_constraint.active) {
      resolved_radius = std::clamp(view_constraint.radius, minimum_radius, resolved_radius);
    }
  }

  const auto camera_has_support_obstruction = [&](const float radius) {
    const Vec3 camera_position = target + offset_for_radius(radius);
    const TerrainSurfaceSample sample =
        support_surfaces_.sample({{camera_position.x, camera_position.z},
                                  camera_position.y,
                                  std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::infinity()});
    return sample.valid && camera_position.y < sample.height + surface_clearance;
  };

  if (!camera_has_support_obstruction(resolved_radius)) {
    return resolved_radius;
  }
  for (float candidate = resolved_radius - search_step; candidate >= lower_bound;
       candidate -= search_step) {
    if (!camera_has_support_obstruction(candidate)) {
      return candidate;
    }
  }
  return lower_bound;
}

void LumenRun::setAvatarPreviewYaw(const float yaw) {
  player_preview_yaw_ = yaw;
  player_preview_yaw_enabled_ = true;
  if (!scene_.objects().empty() && !player_avatar_instance_.object_indices.empty()) {
    updateSceneObjects(1.0f / 60.0f);
  }
}

void LumenRun::clearAvatarPreviewYaw() {
  player_preview_yaw_enabled_ = false;
}

void LumenRun::setPlayerAvatarVisible(const bool visible) {
  player_avatar_visible_ = visible;
  if (visible) {
    player_avatar_pose_valid_ = false;
    return;
  }
  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index < scene_.objects().size()) {
      hideRenderObject(scene_.objects()[object_index]);
    }
  }
}

void LumenRun::setCaveDebugOverlayEnabled(const bool enabled) {
  cave_debug_overlay_enabled_ = enabled;
  updateCaveDebugOverlayVisibility();
}

bool LumenRun::caveDebugOverlayEnabled() const {
  return cave_debug_overlay_enabled_;
}

void LumenRun::setCaveDebugOverlayLayerMask(const std::uint32_t mask) {
  cave_debug_overlay_layer_mask_ = mask;
  updateCaveDebugOverlayVisibility();
}

std::uint32_t LumenRun::caveDebugOverlayLayerMask() const {
  return cave_debug_overlay_layer_mask_;
}

void LumenRun::updateCaveDebugOverlayVisibility() {
  auto &objects = scene_.objects();
  for (const CaveDebugOverlayObject &overlay_entry : cave_debug_overlay_objects_) {
    const std::size_t object_index = overlay_entry.object_index;
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[object_index];
    const bool visible = cave_debug_overlay_enabled_ &&
                         (cave_debug_overlay_layer_mask_ == 0u ||
                          (cave_debug_overlay_layer_mask_ & overlay_entry.layer) != 0u);
    if (visible) {
      object.transform.scale = overlay_entry.visible_scale;
      object.material.opacity = overlay_entry.visible_opacity;
      object.material.emission_strength = overlay_entry.visible_emission;
    } else {
      object.transform.scale = {0.001f, 0.001f, 0.001f};
      object.material.opacity = 0.0f;
      object.material.emission_strength = 0.0f;
    }
  }
}

void LumenRun::setAvatarPointTarget(const Vec3 target) {
  player_avatar_point_target_ = target;
  player_avatar_point_enabled_ = true;
}

bool LumenRun::pointAvatarAtRay(const Vec3 origin, const Vec3 direction, const float max_distance) {
  const MathResult<Vec3> ray_direction_result = safeNormalize(direction);
  if (!ray_direction_result || max_distance <= 0.0f) {
    return false;
  }
  const Vec3 ray_direction = ray_direction_result.value;

  bool found = false;
  float best_distance = max_distance;
  Vec3 best_target{};

  constexpr int support_steps = 128;
  const float step_size = max_distance / static_cast<float>(support_steps);
  bool previous_valid = false;
  float previous_signed_height = 0.0f;
  float previous_t = 0.0f;
  for (int step = 1; step <= support_steps; ++step) {
    const float t = step_size * static_cast<float>(step);
    const Vec3 probe = origin + ray_direction * t;
    const TerrainSurfaceSample sample = support_surfaces_.sample(Vec2{probe.x, probe.z});
    if (!sample.valid) {
      previous_valid = false;
      continue;
    }
    const float signed_height = probe.y - sample.height;
    if (previous_valid && previous_signed_height >= 0.0f && signed_height <= 0.0f) {
      float lo = previous_t;
      float hi = t;
      TerrainSurfaceSample hit_sample = sample;
      for (int refine = 0; refine < 8; ++refine) {
        const float mid = (lo + hi) * 0.5f;
        const Vec3 mid_probe = origin + ray_direction * mid;
        const TerrainSurfaceSample mid_sample =
            support_surfaces_.sample(Vec2{mid_probe.x, mid_probe.z});
        if (!mid_sample.valid) {
          lo = mid;
          continue;
        }
        const float mid_signed_height = mid_probe.y - mid_sample.height;
        if (mid_signed_height > 0.0f) {
          lo = mid;
        } else {
          hi = mid;
          hit_sample = mid_sample;
        }
      }
      const Vec3 hit_probe = origin + ray_direction * hi;
      best_distance = hi;
      best_target = {hit_probe.x, hit_sample.height, hit_probe.z};
      found = true;
      break;
    }
    previous_valid = true;
    previous_signed_height = signed_height;
    previous_t = t;
  }

  PhysicsRayHit physics_hit;
  PhysicsRay ray;
  ray.origin = origin;
  ray.direction = ray_direction;
  ray.max_distance = max_distance;
  ray.filter.collides_with = kPhysicsLayerWorld;
  ray.filter.ignore_body = player_body_;
  if (physics_.raycast(ray, physics_hit) &&
      (!found || physics_hit.distance < best_distance - 0.02f)) {
    best_target = physics_hit.point;
    found = true;
  }

  if (!found) {
    return false;
  }
  setAvatarPointTarget(best_target);
  return true;
}

void LumenRun::clearAvatarPointTarget() {
  player_avatar_point_enabled_ = false;
}

void LumenRun::updateInteractionFocus(const Vec3 ray_origin, const Vec3 ray_direction,
                                      const float dt) {
  std::vector<InteractionTarget> targets;
  targets.reserve(3u + coal_ores_.size() + cave_webs_.size() + cave_skitters_.size());
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  const bool player_near_chest = length(player_position_ - chest_focus) < kChestInteractionDistance;
  std::string action = "Open";
  std::string subject = "Chest";
  if (chest_open_) {
    action = "Take";
    subject = "Items";
    if (const ItemStack *stack = chest_inventory_.slot(chest_selected_slot_);
        stack != nullptr && !stack->empty()) {
      if (const ItemDefinition *definition = item_registry_.find(stack->item_id)) {
        subject = definition->display_name;
      }
    }
  }
  targets.push_back({.id = chest_open_ ? "lumen.supply_chest.contents" : "lumen.supply_chest",
                     .action_graph = chest_open_ ? "action.chest.take" : "action.chest.open",
                     .kind = InteractionTargetKind::Container,
                     .action_label = action,
                     .subject_label = subject,
                     .position = chest_focus,
                     .radius = 0.82f,
                     .max_distance = 14.0f,
                     .proximity_distance = kChestInteractionDistance,
                     .enabled = player_near_chest});

  const Vec3 relay_focus = prismRelayFocusPosition();
  const bool player_near_relay =
      length(player_position_ - relay_focus) <= kPrismRelayInteractionDistance;
  targets.push_back({.id = "lumen.prism_relay",
                     .action_graph = "action.relay.activate",
                     .kind = InteractionTargetKind::Item,
                     .action_label = prism_relay_active_ ? "Tune" : "Ignite",
                     .subject_label = "Prism Relay",
                     .position = relay_focus,
                     .radius = 0.92f,
                     .max_distance = 14.0f,
                     .proximity_distance = kPrismRelayInteractionDistance,
                     .enabled = player_near_relay});

  focused_cave_web_index_ = 0;
  focused_cave_web_valid_ = false;
  const float ray_length = length(ray_direction);
  const Vec3 ray_direction_unit = ray_length > 0.0001f ? ray_direction / ray_length : Vec3{};
  const auto webRayHit = [&](const CaveWebObstacle &web) -> std::optional<std::pair<float, Vec3>> {
    if (web.broken || ray_length <= 0.0001f) {
      return std::nullopt;
    }
    const float denom = dot(ray_direction_unit, web.normal);
    if (std::abs(denom) <= 0.0001f) {
      return std::nullopt;
    }
    const float t = dot(web.center - ray_origin, web.normal) / denom;
    if (t < 0.0f || t > kCaveWebInteractionDistance + 1.0f) {
      return std::nullopt;
    }
    const Vec3 hit = ray_origin + ray_direction_unit * t;
    const Vec3 offset = hit - web.center;
    const float x = dot(offset, web.side) / std::max(web.radius_x, 0.001f);
    const float y = dot(offset, web.up) / std::max(web.radius_y, 0.001f);
    if (x * x + y * y > 1.0f) {
      return std::nullopt;
    }
    return std::make_pair(t, hit);
  };
  const auto rayOccludedByWeb = [&](const Vec3 target, const float target_distance) {
    if (ray_length <= 0.0001f) {
      return false;
    }
    for (const CaveWebObstacle &web : cave_webs_) {
      if (web.broken) {
        continue;
      }
      const std::optional<std::pair<float, Vec3>> hit = webRayHit(web);
      if (!hit.has_value() || hit->first <= 0.02f) {
        continue;
      }
      const Vec3 to_target = target - ray_origin;
      const float to_target_length = length(to_target);
      if (to_target_length <= 0.0001f ||
          dot(to_target / to_target_length, ray_direction_unit) <= 0.92f) {
        continue;
      }

      const Vec3 web_offset = target - web.center;
      const float web_plane = dot(web_offset, web.normal);
      const float web_x = dot(web_offset, web.side) / std::max(web.radius_x, 0.001f);
      const float web_y = dot(web_offset, web.up) / std::max(web.radius_y, 0.001f);
      const bool inside_web_span = web_x * web_x + web_y * web_y <= 1.08f;
      const float attached_depth = std::max(web.thickness * 1.8f, 0.20f);
      const bool attached_to_web =
          inside_web_span && std::abs(web_plane) <= attached_depth &&
          hit->first <= target_distance + attached_depth;
      const bool behind_web = hit->first < std::max(target_distance - 0.02f, 0.0f);
      if (behind_web || attached_to_web) {
        return true;
      }
    }
    return false;
  };
  const ItemStack &equipped = equipment_.equipped();
  const ItemDefinition *equipped_definition = item_registry_.find(equipped.item_id);
  const bool pickaxe_equipped =
      equipped_definition != nullptr && equipped_definition->has_mining_tool;
  for (std::size_t i = 0; i < coal_ores_.size(); ++i) {
    const CoalOreNode &ore = coal_ores_[i];
    if (ore.collected) {
      continue;
    }
    const float distance = length(player_position_ - ore.position);
    targets.push_back({.id = "lumen.coal_ore." + std::to_string(i),
                       .action_graph = "action.mine.coal_ore",
                       .kind = InteractionTargetKind::Item,
                       .action_label = pickaxe_equipped ? "Mine" : "Need",
                       .subject_label = pickaxe_equipped ? "Coal Ore" : "Pickaxe",
                       .position = ore.position,
                       .radius = std::max(ore.radius, 0.28f),
                       .max_distance = 14.0f,
                       .user_data = static_cast<std::uint64_t>(i),
                       .enabled = distance <= kOreInteractionDistance});
  }
  float nearest_web_distance = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < cave_webs_.size(); ++i) {
    const CaveWebObstacle &web = cave_webs_[i];
    if (web.broken) {
      continue;
    }
    const float distance = length(player_position_ - web.center);
    if (distance < nearest_web_distance && distance <= kCaveWebInteractionDistance) {
      nearest_web_distance = distance;
      focused_cave_web_index_ = i;
      focused_cave_web_valid_ = true;
    }
    const std::optional<std::pair<float, Vec3>> web_hit = webRayHit(web);
    targets.push_back({.id = web.id.empty() ? "lumen.cave_web." + std::to_string(i) : web.id,
                       .action_graph = "action.mine.cave_web",
                       .kind = InteractionTargetKind::Item,
                       .shape = web_hit.has_value() ? InteractionTargetShape::ExplicitHit
                                                     : InteractionTargetShape::Sphere,
                       .action_label = pickaxe_equipped ? "Cut" : "Need",
                       .subject_label = pickaxe_equipped ? "Spider Web" : "Pickaxe",
                       .position = web_hit.has_value() ? web_hit->second : web.center,
                       .radius = std::max(std::max(web.radius_x, web.radius_y) * 0.56f, 0.42f),
                       .max_distance = 14.0f,
                       .proximity_distance = kCaveWebInteractionDistance,
                       .hit_distance = web_hit.has_value() ? web_hit->first : 0.0f,
                       .evidence_strength = web_hit.has_value() ? 2.0f : 1.0f,
                       .user_data = static_cast<std::uint64_t>(i),
                       .enabled = distance <= kCaveWebInteractionDistance});
  }
  for (std::size_t i = 0; i < cave_skitters_.size(); ++i) {
    const CaveSkitter &skitter = cave_skitters_[i];
    if (skitter.dead || skitter.state.dead) {
      continue;
    }
    const float distance = length(player_position_ - skitter.state.position);
    const Vec3 skitter_focus = skitter.state.position + Vec3{0.0f, 0.10f, 0.0f};
    const float ray_distance = length(skitter_focus - ray_origin);
    targets.push_back({.id = skitter.id.empty() ? "lumen.cave_skitter." + std::to_string(i)
                                                : skitter.id,
                       .action_graph = "action.mine.cave_skitter",
                       .kind = InteractionTargetKind::Item,
                       .action_label = pickaxe_equipped ? "Strike" : "Need",
                       .subject_label = pickaxe_equipped ? "Cave Skitter" : "Pickaxe",
                       .position = skitter_focus,
                       .radius = 0.48f,
                       .max_distance = 14.0f,
                       .proximity_distance = kCaveSkitterInteractionDistance,
                       .user_data = static_cast<std::uint64_t>(i),
                       .occluded = rayOccludedByWeb(skitter_focus, ray_distance),
                       .enabled = distance <= kCaveSkitterInteractionDistance});
  }

  interaction_.update(targets, ray_origin, ray_direction, player_position_, dt);
}

void LumenRun::interactFocused() {
  const InteractionFocus &focus = interaction_.focus();
  if (!focus.visible) {
    return;
  }

  if (focus.kind == InteractionTargetKind::Container &&
      focus.action_graph == "action.chest.open") {
    openChest();
    setAvatarPointTarget(chest_base_ + Vec3{0.0f, 0.42f, 0.0f});
    return;
  }
  if (focus.kind == InteractionTargetKind::Container &&
      focus.action_graph == "action.chest.take") {
    if (takeChestSlot(chest_selected_slot_)) {
      setAvatarPointTarget(focus.position);
    }
    return;
  }
  if (focus.kind == InteractionTargetKind::Item &&
      focus.action_graph == "action.relay.activate") {
    activatePrismRelay();
    setAvatarPointTarget(prismRelayFocusPosition());
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.action_graph == "action.mine.coal_ore") {
    (void)mineFocusedOre(static_cast<std::size_t>(focus.user_data));
    return;
  }
  if (focus.kind == InteractionTargetKind::Item && focus.action_graph == "action.mine.cave_web") {
    (void)mineFocusedCaveWeb(static_cast<std::size_t>(focus.user_data));
    return;
  }
  if (focus.kind == InteractionTargetKind::Item &&
      focus.action_graph == "action.mine.cave_skitter") {
    (void)mineFocusedCaveSkitter(static_cast<std::size_t>(focus.user_data));
  }
}

void LumenRun::secondaryInteractFocused(const Vec3 ray_origin, const Vec3 ray_direction) {
  const InteractionFocus &focus = interaction_.focus();
  if (focus.visible && focus.kind == InteractionTargetKind::Container &&
      focus.action_graph == "action.chest.open") {
    interactFocused();
    return;
  }
  if (placeEquippedResource(ray_origin, ray_direction)) {
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.action_graph == "action.mine.coal_ore") {
    (void)mineFocusedOre(static_cast<std::size_t>(focus.user_data));
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.action_graph == "action.mine.cave_web") {
    (void)mineFocusedCaveWeb(static_cast<std::size_t>(focus.user_data));
    return;
  }
  if (focus.visible && focus.kind == InteractionTargetKind::Item &&
      focus.action_graph == "action.mine.cave_skitter") {
    (void)mineFocusedCaveSkitter(static_cast<std::size_t>(focus.user_data));
  }
}

void LumenRun::openChest() {
  chest_open_ = true;
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  chest_distance_close_armed_ = length(player_position_ - chest_focus) <= kChestAutoCloseDistance;
  chest_lid_animation_.setTarget(1.0f);
  for (std::size_t i = 0; i < chest_inventory_.slotCount(); ++i) {
    const ItemStack *stack = chest_inventory_.slot(i);
    if (stack != nullptr && !stack->empty()) {
      chest_selected_slot_ = i;
      break;
    }
  }
}

void LumenRun::closeChest() {
  chest_open_ = false;
  chest_distance_close_armed_ = false;
  chest_lid_animation_.setTarget(0.0f);
}

bool LumenRun::takeChestItem(const std::string_view item_id) {
  const ItemDefinition *definition = item_registry_.find(item_id);
  if (definition == nullptr) {
    return false;
  }
  const std::optional<std::size_t> slot = hotbar_.addItem(*definition, 1);
  if (!slot.has_value()) {
    return false;
  }
  (void)chest_inventory_.removeItem(item_id, 1);
  for (ChestItemVisual &item : chest_items_) {
    if (item.item_id == item_id && item.available) {
      item.available = false;
      break;
    }
  }
  if (hotbar_.selectedStack() == nullptr || hotbar_.selectedStack()->empty()) {
    (void)hotbar_.select(*slot);
  }
  equipment_.equipFromHotbar(hotbar_);
  for (std::size_t i = 0; i < chest_inventory_.slotCount(); ++i) {
    const ItemStack *stack = chest_inventory_.slot(i);
    if (stack != nullptr && !stack->empty()) {
      chest_selected_slot_ = i;
      return true;
    }
  }
  return true;
}

bool LumenRun::takeChestSlot(const std::size_t slot_index) {
  const ItemStack *stack = chest_inventory_.slot(slot_index);
  if (stack == nullptr || stack->empty()) {
    return false;
  }
  chest_selected_slot_ = slot_index;
  return takeChestItem(stack->item_id);
}

bool LumenRun::takeSupplyTorch() {
  if (!supplyCrateNearby()) {
    return false;
  }
  const ItemDefinition *definition = item_registry_.find("torch");
  if (definition == nullptr) {
    return false;
  }
  const std::optional<std::size_t> slot = hotbar_.addItem(*definition, 1);
  if (!slot.has_value()) {
    return false;
  }
  if (equipment_.isEquipped("torch")) {
    equipment_.equipFromHotbar(hotbar_);
  }
  return true;
}

void LumenRun::selectHotbarSlot(const std::size_t index) {
  if (hotbar_.select(index)) {
    equipment_.equipFromHotbar(hotbar_);
  }
}

bool LumenRun::chestInterfaceOpen() const {
  return chest_open_ && chest_lid_animation_.value() > 0.35f;
}

bool LumenRun::supplyCrateNearby() const {
  const Vec3 supply_focus = supply_crate_base_ + Vec3{0.0f, 0.42f, 0.0f};
  return length(player_position_ - supply_focus) <= kSupplyCrateInteractionDistance;
}

int LumenRun::torchCount() const {
  int count = 0;
  for (const ItemStack &stack : hotbar_.slots()) {
    if (stack.item_id == "torch") {
      count += std::max(stack.quantity, 0);
    }
  }
  return count;
}

Vec3 LumenRun::supplyCratePosition() const {
  return supply_crate_base_;
}

FocusPromptModel LumenRun::focusPromptModel() const {
  const InteractionFocus &focus = interaction_.focus();
  return {.visible = focus.visible,
          .alpha = focus.strength,
          .key = "E",
          .action = focus.action_label,
          .subject = focus.subject_label};
}

HotbarHudModel LumenRun::hotbarHudModel() const {
  HotbarHudModel model;
  model.visible = true;
  const std::vector<ItemStack> &slots = hotbar_.slots();
  model.slots.reserve(slots.size());
  for (std::size_t i = 0; i < slots.size(); ++i) {
    HotbarSlotModel slot_model;
    slot_model.key = std::to_string(i + 1u);
    slot_model.selected = i == hotbar_.selectedIndex();
    if (const ItemDefinition *definition = item_registry_.find(slots[i].item_id)) {
      slot_model.filled = !slots[i].empty();
      slot_model.label = definition->short_label;
      slot_model.tint = definition->tint;
      if (slots[i].quantity > 1) {
        slot_model.quantity = std::to_string(slots[i].quantity);
      }
    }
    model.slots.push_back(std::move(slot_model));
  }
  return model;
}

ChestContentsHudModel LumenRun::chestContentsHudModel() const {
  ChestContentsHudModel model;
  model.visible = chestInterfaceOpen();
  model.title = "Chest";
  const std::vector<ItemStack> &slots = chest_inventory_.slots();
  model.slots.reserve(slots.size());
  for (std::size_t i = 0; i < slots.size(); ++i) {
    ChestContentsSlotModel slot_model;
    slot_model.selected = i == chest_selected_slot_;
    if (const ItemDefinition *definition = item_registry_.find(slots[i].item_id)) {
      slot_model.filled = !slots[i].empty();
      slot_model.label = definition->short_label;
      slot_model.tint = definition->tint;
      if (slots[i].quantity > 1) {
        slot_model.quantity = std::to_string(slots[i].quantity);
      }
    }
    model.slots.push_back(std::move(slot_model));
  }
  return model;
}

std::optional<DynamicPointLight> LumenRun::equippedLight() const {
  return equipped_light_.active ? std::optional<DynamicPointLight>{equipped_light_} : std::nullopt;
}

std::optional<DynamicPointLight> LumenRun::pondAccentLight() const {
  if (!pond_accent_light_valid_) {
    return std::nullopt;
  }
  return evaluateFlickerLight({.color = {1.0f, 0.48f, 0.20f},
                               .intensity = 1.18f,
                               .amplitude = 0.035f,
                               .speed = 3.2f,
                               .source_radius = 1.65f},
                              pond_accent_light_position_, status_.elapsed_seconds);
}

std::optional<DynamicPointLight> LumenRun::prismRelayLight() const {
  if (prism_relay_charge_ <= 0.05f) {
    return std::nullopt;
  }
  return evaluateFlickerLight({.color = {0.46f, 0.86f, 1.0f},
                              .intensity = 2.85f * clamp(prism_relay_charge_, 0.0f, 1.15f),
                               .amplitude = 0.045f,
                               .speed = 2.4f,
                               .source_radius = 4.8f},
                              prismRelayFocusPosition(), status_.elapsed_seconds);
}

CaveLightingState LumenRun::caveLightingState() const {
  return caveLightingStateAt(player_position_);
}

bool LumenRun::classicGauntletActive() const {
  return classic_gauntlet_active_;
}

const AutomapModel &LumenRun::classicGauntletAutomap() const {
  return classic_gauntlet_automap_;
}

ClassicHudSignalModel LumenRun::classicHudSignals() const {
  return classic_gauntlet_hud_;
}

TransitionWipeFrame LumenRun::classicTransitionWipe() const {
  return classic_gauntlet_wipe_.frame();
}

Vec3 LumenRun::classicGauntletEntryPosition() const {
  return classic_gauntlet_entry_;
}

Vec3 LumenRun::classicGauntletLookTarget() const {
  if (length(classic_gauntlet_door_center_) > 0.0001f &&
      length(classic_gauntlet_lift_base_) > 0.0001f) {
    return classic_gauntlet_door_center_ +
           (classic_gauntlet_lift_base_ - classic_gauntlet_door_center_) * 0.76f +
           Vec3{0.0f, 0.28f, 0.0f};
  }
  return length(classic_gauntlet_exit_) <= 0.0001f ? player_position_ : classic_gauntlet_exit_;
}

float LumenRun::classicGauntletCameraYaw() const {
  const Vec3 target = classicGauntletLookTarget();
  const Vec3 delta = target - classic_gauntlet_entry_;
  return std::atan2(delta.x, delta.z);
}

const LumenRun::AuthoredCaveSection *
LumenRun::caveSectionAt(const Vec3 position, CaveInteriorSample *sample) const {
  const AuthoredCaveSection *best_section = nullptr;
  CaveInteriorSample best_sample{};
  for (const AuthoredCaveSection &section : cave_sections_) {
    const CaveInteriorSample candidate = sampleCaveInteriorVolume(section.tunnel, position);
    if (best_section == nullptr || candidate.interior > best_sample.interior) {
      best_section = &section;
      best_sample = candidate;
    }
  }
  if (sample != nullptr) {
    *sample = best_sample;
  }
  return best_section;
}

CaveLightingState LumenRun::caveLightingStateAt(const Vec3 position) const {
  if (forced_spawn_lighting_frames_ > 0) {
    return {};
  }
  if (cave_sections_.empty()) {
    return {};
  }
  CaveInteriorSample best_sample{};
  (void)caveSectionAt(position, &best_sample);

  struct LightCandidate {
    CaveWallLightSample sample{};
    float distance = 0.0f;
    float score = 0.0f;
  };
  std::vector<LightCandidate> candidates;
  candidates.reserve(24u);

  const auto append_authored_fixtures = [&](const CaveInteriorSample &section_sample,
                                            const std::vector<CaveWallFixturePlacement> &fixtures,
                                            const float path_length) {
    constexpr float kProgressWindow = 0.18f;
    constexpr float kMinimumProgressGain = 0.62f;
    for (const CaveWallFixturePlacement &fixture : fixtures) {
      const float distance = length(fixture.light_position - position);
      const float progress_delta = std::abs(fixture.t - section_sample.tunnel_t);
      const float progress_gain = 1.0f - (1.0f - kMinimumProgressGain) *
                                             clamp(progress_delta / kProgressWindow, 0.0f, 1.0f);
      const float weight = clamp(section_sample.interior * progress_gain, 0.0f, 1.0f);
      candidates.push_back(
          {.sample = {.position = fixture.light_position,
                      .color = fixture.light_color,
                      .intensity = kAuthoredCaveFixtureIntensity * weight,
                      .source_radius = kAuthoredCaveFixtureSourceRadius,
                      .tunnel_t = fixture.t},
           .distance = distance,
           .score = distance * 0.55f + progress_delta * std::max(path_length, 1.0f) * 0.10f});
    }
  };
  float entrance_light = 0.0f;
  for (const AuthoredCaveSection &section : cave_sections_) {
    const CaveInteriorSample section_sample = sampleCaveInteriorVolume(section.tunnel, position);
    const float path_length = estimateCaveTunnelLength(section.tunnel);
    if (section.contributes_entrance_light) {
      entrance_light = std::max(entrance_light, section_sample.entrance_light);
    }
    append_authored_fixtures(section_sample, section.wall_fixtures, path_length);
    append_authored_fixtures(section_sample, section.secondary_wall_fixtures, path_length);
  }
  for (const CoalOreNode &ore : coal_ores_) {
    if (ore.collected || ore.hit_flash <= 0.001f) {
      continue;
    }
    const float distance = length(ore.position - position);
    if (distance > 5.5f) {
      continue;
    }
    candidates.push_back({.sample = {.position = ore.position + ore.normal * 0.10f,
                                     .color = {0.95f, 0.12f, 0.04f},
                                     .intensity = ore.hit_flash * 0.34f,
                                     .source_radius = std::max(ore.radius * 1.4f, 0.24f),
                                     .tunnel_t = best_sample.tunnel_t},
                          .distance = distance,
                          .score = distance * 0.92f + 2.0f});
  }
  for (const CaveSkitter &skitter : cave_skitters_) {
    if (skitter.dead) {
      continue;
    }
    const float distance = length(skitter.state.position - position);
    if (distance > 7.0f) {
      continue;
    }
    candidates.push_back(
        {.sample = {.position = skitter.state.position + Vec3{0.0f, 0.13f, 0.0f},
                    .color = {0.78f, 0.08f, 0.035f},
                    .intensity = 0.26f + skitter.bite_flash * 0.42f + skitter.hit_flash * 0.18f,
                    .source_radius = 0.32f,
                    .tunnel_t = best_sample.tunnel_t},
         .distance = distance,
         .score = distance * 0.80f + 0.70f});
  }

  std::sort(
      candidates.begin(), candidates.end(),
      [](const LightCandidate &lhs, const LightCandidate &rhs) { return lhs.score < rhs.score; });

  Vec3 wall_light_position = cave_entrance_light_position_;
  Vec3 wall_light_color = kCaveIndustrialRedLight;
  std::vector<CaveWallLightSample> wall_lights;
  wall_lights.reserve(candidates.size());
  for (const LightCandidate &candidate : candidates) {
    if (wall_lights.empty()) {
      wall_light_position = candidate.sample.position;
      wall_light_color = candidate.sample.color;
    }
    wall_lights.push_back(candidate.sample);
  }

  const float interior = best_sample.interior;
  float wall_light = entrance_light * 0.20f;
  for (const LightCandidate &candidate : candidates) {
    const float distance_sq =
        std::max(dot(candidate.sample.position - position, candidate.sample.position - position),
                 0.0001f);
    const float softened =
        std::max(distance_sq, candidate.sample.source_radius * candidate.sample.source_radius);
    wall_light = std::max(wall_light, candidate.sample.intensity / softened * 0.035f);
  }
  wall_light = clamp(wall_light, 0.0f, 1.0f);
  return {.interior = interior,
          .entrance_light = entrance_light,
          .depth = best_sample.depth * best_sample.interior,
          .chamber = best_sample.chamber * best_sample.interior,
          .entrance_light_position = cave_entrance_light_position_,
          .wall_light = wall_light,
          .wall_light_position = wall_light_position,
          .wall_light_color = wall_light_color,
          .wall_lights = std::move(wall_lights)};
}

} // namespace aster
