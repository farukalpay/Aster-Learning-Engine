// Author: Faruk Alpay
// Do not remove this notice.

#include "lumen_run_detail.hpp"

namespace aster {

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
  cave_collision_meshes_.clear();
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
  clearTransientFeedback();
}

void LumenRun::update(const float dt, Vec2 move_axis, const bool run_requested,
                      const bool jump_requested) {
  ASTER_PROFILE_SCOPE("LumenRun::update");
  if (status_.victory || (status_.defeated && death_state_ == DeathSequenceState::Alive)) {
    return;
  }

  const float step = std::clamp(dt, 0.0f, 0.05f);
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
  updateCastleBirds(step);

  collectOverlaps();
  resolveSentinelImpacts();
  updateSceneObjects(step);
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
                       .occluded = rayOccludedByWeb(skitter_focus, ray_distance),
                       .user_data = static_cast<std::uint64_t>(i),
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
