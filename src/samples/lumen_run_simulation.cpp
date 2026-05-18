// Author: Faruk Alpay
// Do not remove this notice.

#include "lumen_run_detail.hpp"

namespace aster {

// Frame simulation, actor motion, transient visuals, and world-state maintenance.
void LumenRun::applyCaveWebTraversalGates(PhysicsBody &body) {
  for (const CaveWebObstacle &web : cave_webs_) {
    if (web.broken) {
      continue;
    }
    const Vec3 normal =
        length(web.normal) > 0.0001f ? normalize(web.normal) : Vec3{0.0f, 0.0f, -1.0f};
    const Vec3 side =
        length(web.side) > 0.0001f ? normalize(web.side) : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 up = length(web.up) > 0.0001f ? normalize(web.up) : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 offset = body.position - web.center;
    const float radius_x = std::max(web.radius_x + tuning_.player_radius * 0.80f, 0.001f);
    const float radius_y = std::max(web.radius_y + playerSupportExtent() * 0.38f, 0.001f);
    const float x = dot(offset, side) / radius_x;
    const float y = dot(offset, up) / radius_y;
    if (x * x + y * y > 1.0f) {
      continue;
    }

    const float approach = dot(offset, normal);
    const float clearance =
        tuning_.player_radius + std::max(web.thickness * 0.50f, tuning_.player_radius * 0.25f);
    if (std::abs(approach) > clearance) {
      continue;
    }

    const float through_speed = dot(body.velocity, normal);
    if (std::abs(through_speed) > 0.001f) {
      body.velocity = body.velocity - normal * (through_speed * 0.58f);
      physics_.wakeBody(player_body_);
    }
  }
}

void LumenRun::updatePlayerPhysics(const float dt, const Vec2 move_axis, const bool run_requested,
                                   const bool jump_requested) {
  ASTER_PROFILE_SCOPE("LumenRun::updatePlayerPhysics");
  if (!physics_.valid(player_body_)) {
    return;
  }

  const PlayerMovePlan move_plan = buildPlayerMovePlan(
      {.walk_speed = tuning_.player_speed,
       .run_multiplier = tuning_.run_multiplier,
       .response_rate = tuning_.response_rate,
       .ground_probe_distance = 0.18f,
       .max_slope_angle = radians(52.0f),
       .jump_speed = tuning_.jump_speed},
      {.move_axis = move_axis, .run_requested = run_requested, .jump_requested = jump_requested});

  CharacterMoveInput character_input = move_plan.input;
  const float cave_web_slow = caveWebSlowScaleAt(physics_.body(player_body_).position);
  if (cave_web_slow < 0.999f) {
    character_input.desired_velocity.x *= cave_web_slow;
    character_input.desired_velocity.z *= cave_web_slow;
    PhysicsBody &body = physics_.body(player_body_);
    const float damp_response = 1.0f - std::exp(-16.0f * std::max(dt, 0.0f));
    const float velocity_scale = 1.0f - (1.0f - cave_web_slow) * damp_response;
    body.velocity.x *= velocity_scale;
    body.velocity.z *= velocity_scale;
    physics_.wakeBody(player_body_);
  }
  const bool was_climbing = player_climbing_;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  const PhysicsFluidSample fluid_sample = physics_.sampleFluid(player_body_);
  const SwimMotionResult swim_motion = buildSwimMotion(
      fluid_sample, physics_.body(player_body_).velocity,
      {.desired_velocity = character_input.desired_velocity, .ascend_requested = jump_requested},
      {.activation_submersion = 0.22f,
       .full_swim_submersion = 0.62f,
       .horizontal_speed_scale = 0.64f,
       .flow_influence = 0.34f,
       .surface_clearance = 0.038f,
       .float_response = 8.0f,
       .max_upward_speed = 1.22f,
       .max_downward_speed = 0.48f,
       .ascend_speed = 1.35f});
  player_swimming_ = swim_motion.swimming;
  player_swim_blend_ = swim_motion.blend;
  if (swim_motion.swimming) {
    character_input.desired_velocity = swim_motion.desired_velocity;
    character_input.jump_requested = false;
    PhysicsBody &body = physics_.body(player_body_);
    const float response = 1.0f - std::exp(-9.0f * std::max(dt, 0.0f));
    body.velocity.y += (swim_motion.target_vertical_velocity - body.velocity.y) * response;
    physics_.wakeBody(player_body_);
  }

  ClimbMotionResult climb_motion;
  if (!swim_motion.swimming) {
    const ClimbSurfaceSample climb_surface =
        sampleClimbableCylinder(climbable_tree_, physics_.body(player_body_).position,
                                {.capture_distance = 0.42f,
                                 .character_clearance = tuning_.player_radius * 1.22f,
                                 .stick_response = 12.0f,
                                 .tangent_speed_scale = 0.46f,
                                 .ascend_speed = 1.62f,
                                 .hold_vertical_speed = 0.16f,
                                 .max_correction = 0.16f});
    climb_motion = buildClimbMotion(
        climb_surface,
        {.desired_velocity = character_input.desired_velocity,
         .engage_requested = jump_requested || (was_climbing && length(move_axis) > 0.05f),
         .ascend_requested = jump_requested},
        {.capture_distance = 0.42f,
         .character_clearance = tuning_.player_radius * 1.22f,
         .stick_response = 12.0f,
         .tangent_speed_scale = 0.46f,
         .ascend_speed = 1.62f,
         .hold_vertical_speed = 0.16f,
         .max_correction = 0.16f});
    if (climb_motion.climbing) {
      PhysicsBody &body = physics_.body(player_body_);
      body.position = body.position + climb_motion.position_correction;
      body.velocity = climb_motion.desired_velocity;
      character_input.desired_velocity = climb_motion.desired_velocity;
      character_input.jump_requested = false;
      player_climbing_ = true;
      player_climb_blend_ = climb_motion.blend;
      physics_.wakeBody(player_body_);
    }
  }

  const TerrainSurfaceQuerySampler surface_sampler = [this](const Vec3 support_position) {
    if (isSwimmableWater(support_position)) {
      return TerrainSurfaceSample{};
    }
    return sampleWorldSupport(
        {{support_position.x, support_position.z}, support_position.y, 0.22f, 1.35f});
  };
  const CharacterMoveResult character_state =
      moveCharacterOnSurface(physics_, player_body_, surface_sampler, character_input,
                             {.controller = move_plan.character_settings,
                              .support_extent_y = playerSupportExtent(),
                              .support_radius = tuning_.player_radius,
                              .support_sample_count = 10},
                             dt);
  player_grounded_ = character_state.grounded && !player_swimming_ && !player_climbing_;
  if (!player_swimming_ && !player_climbing_) {
    const CharacterMoveResult step_assist = applySurfaceStepAssist(
        physics_, player_body_, surface_sampler, character_input.desired_velocity,
        {.support_extent_y = playerSupportExtent(),
         .support_radius = tuning_.player_radius,
         .support_sample_count = 10,
         .probe_distance = tuning_.player_radius * 1.65f,
         .max_step_up = 0.34f,
         .max_step_down = 0.12f,
         .min_horizontal_speed = 0.20f,
         .skin_width = 0.010f});
    player_grounded_ = player_grounded_ || step_assist.grounded;
  }

  physics_.step(dt);
  [[maybe_unused]] const bool horizontal_collision_resolved = resolveContinuousHorizontalCollision(
      physics_, player_body_,
      {.radius = tuning_.player_radius, .collision_mask = kPhysicsLayerWorld});
  if (player_climbing_ && physics_.valid(player_body_)) {
    PhysicsBody &body = physics_.body(player_body_);
    body.velocity.y = std::max(body.velocity.y, climb_motion.desired_velocity.y);
  }
  if (!player_swimming_ && !player_climbing_) {
    const CharacterMoveResult terrain_contact =
        solveSurfaceContact(physics_, player_body_, surface_sampler,
                            {.support_extent_y = playerSupportExtent(),
                             .skin_width = 0.010f,
                             .max_correction_distance = 2.40f,
                             .support_radius = tuning_.player_radius,
                             .support_sample_count = 10});
    player_grounded_ = player_grounded_ || terrain_contact.grounded;
  }
  if (!player_swimming_ && !player_climbing_ && physics_.valid(player_body_)) {
    PhysicsBody &body = physics_.body(player_body_);
    CaveTraversalConstraint cave_constraint{};
    for (const AuthoredCaveSection &section : cave_sections_) {
      const CaveTraversalConstraint candidate =
          constrainCaveTraversalPosition(section.tunnel, body.position, tuning_.player_radius);
      if (candidate.active &&
          (!cave_constraint.active ||
           length(candidate.correction) > length(cave_constraint.correction))) {
        cave_constraint = candidate;
      }
    }
    if (cave_constraint.active && length(cave_constraint.correction) > 0.0001f) {
      const Vec3 correction_direction = normalize(cave_constraint.correction);
      const float outward_speed = dot(body.velocity, correction_direction);
      body.position = cave_constraint.corrected_position;
      if (outward_speed < 0.0f) {
        body.velocity = body.velocity - correction_direction * outward_speed;
      }
      physics_.wakeBody(player_body_);
    }
    applyCaveWebTraversalGates(body);
  }

  player_position_ = physics_.body(player_body_).position;
  player_velocity_ = physics_.body(player_body_).velocity;
  player_mouth_open_ =
      player_swimming_
          ? 0.32f
          : (player_climbing_ ? 0.18f
                              : ((!player_grounded_ || player_velocity_.y > 0.65f) ? 1.0f : 0.0f));
  if (length(move_axis) > 0.001f) {
    player_facing_yaw_ = std::atan2(move_axis.x, move_axis.y);
  }
}

void LumenRun::updateSceneObjects(const float animation_dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateSceneObjects");
  auto &objects = scene_.objects();

  const bool preview = player_preview_yaw_enabled_;
  const AvatarPose player_pose = updateAvatarAnimator(
      player_avatar_animator_, {.max_stride_amplitude = 0.62f, .vertical_bob_amplitude = 0.017f},
      {.position = avatarPosePosition(),
       .velocity = preview ? Vec3{} : player_velocity_,
       .desired_facing_yaw = preview ? player_preview_yaw_ : player_facing_yaw_,
       .has_facing_target = true,
       .max_planar_speed = tuning_.player_speed * tuning_.run_multiplier,
       .head_yaw_offset = 0.0f,
       .head_pitch_offset = 0.0f,
       .has_attention_target = player_avatar_point_enabled_ && !preview,
       .attention_target = player_avatar_point_target_,
       .pointing_enabled = player_avatar_point_enabled_ && !preview,
       .mouth_open = preview ? 0.0f : player_mouth_open_,
       .swim_blend = preview ? 0.0f : player_swim_blend_,
       .climb_blend = preview ? 0.0f : player_climb_blend_},
      animation_dt);
  player_avatar_pose_ = player_pose;
  player_avatar_pose_valid_ = true;
  player_render_position_ = player_position_;
  player_render_position_valid_ = true;
  applyAvatarPose(scene_, player_avatar_, player_avatar_instance_, player_pose);
  if (death_state_ != DeathSequenceState::Alive) {
    updateDeathVisuals();
  } else {
    restorePlayerEyeObjects();
  }
  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index < objects.size() &&
        objects[object_index].material.surface_pattern == SurfacePattern::FurFibers) {
      objects[object_index].material.emission_strength = invulnerability_ > 0.0f ? 0.12f : 0.0f;
    }
  }

  const float pulse = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 4.2f);
  for (Shard &shard : shards_) {
    RenderObject &object = objects[shard.object_index];
    if (shard.collected) {
      object.transform.position = {0.0f, -20.0f, 0.0f};
      object.transform.scale = {0.001f, 0.001f, 0.001f};
      object.material.emission_strength = 0.0f;
      continue;
    }
    object.transform.position = shard.position + Vec3{0.0f, pulse * 0.05f, 0.0f};
    object.transform.rotation =
        quatFromEulerXyz({0.0f, status_.elapsed_seconds * 0.7f + shard.position.x, 0.0f});
    object.transform.scale = {tuning_.shard_radius * 0.82f, tuning_.shard_radius * 1.55f,
                              tuning_.shard_radius * 0.82f};
    object.material.emission_strength = 0.30f + pulse * 0.22f;
  }

  for (Sentinel &sentinel : sentinels_) {
    const float angle = sentinel.phase + status_.elapsed_seconds * tuning_.sentinel_speed;
    const float weave = std::sin(angle * 1.7f) * 0.55f;
    RenderObject &object = objects[sentinel.object_index];
    object.transform.position = {
        std::cos(angle) * sentinel.orbit_radius,
        tuning_.sentinel_radius * 1.42f,
        std::sin(angle + weave) * sentinel.orbit_radius,
    };
    object.transform.rotation =
        quatFromEulerXyz({0.06f * std::sin(angle * 1.3f), -angle, 0.04f * std::cos(angle)});
    object.material.emission_strength = 0.18f + 0.08f * std::sin(status_.elapsed_seconds * 5.0f);
  }

  for (std::size_t i = 0; i < rim_objects_.size(); ++i) {
    RenderObject &object = objects[rim_objects_[i]];
    const float phase = status_.elapsed_seconds * 2.0f + static_cast<float>(i) * 0.31f;
    object.material.emission_strength = 0.02f + 0.03f * (0.5f + 0.5f * std::sin(phase));
  }

  updatePrismRelayVisuals(animation_dt);
  updateChestVisuals(animation_dt);
  updateEquipmentVisuals(animation_dt);
  updateCaveVisuals(animation_dt);
  updateCaveSkitterVisuals(animation_dt);
  updateFishingVisual();
  updateCastleBirdVisuals();
  updateCrocodileVisual();
}

float LumenRun::caveWebSlowScaleAt(const Vec3 position) const {
  float slow_scale = 1.0f;
  for (const CaveWebObstacle &web : cave_webs_) {
    if (web.broken) {
      continue;
    }
    const Vec3 offset = position - web.center;
    const float plane_distance = std::abs(dot(offset, web.normal));
    if (plane_distance > std::max(web.thickness, 0.01f)) {
      continue;
    }
    const float radius_x = std::max(web.radius_x, 0.001f);
    const float radius_y = std::max(web.radius_y, 0.001f);
    const float x = dot(offset, web.side) / radius_x;
    const float y = dot(offset, web.up) / radius_y;
    if (x * x + y * y <= 1.0f) {
      slow_scale = std::min(slow_scale, std::clamp(web.slow_scale, 0.05f, 1.0f));
    }
  }
  return slow_scale;
}

TerrainSurfaceSample LumenRun::sampleCaveFloorSupport(const SurfaceSupportQuery &query) const {
  if (!std::isfinite(query.reference_y) || cave_sections_.empty()) {
    return {};
  }

  CaveInteriorSample cave_sample{};
  const Vec3 reference{query.world_position.x, query.reference_y, query.world_position.y};
  const AuthoredCaveSection *section = caveSectionAt(reference, &cave_sample);
  if (section == nullptr) {
    return {};
  }

  const bool inside_cave_plan =
      cave_sample.tunnel_t >= section->tunnel.collision_start_t - 0.04f &&
      cave_sample.lateral <= cave_sample.half_width * 1.28f &&
      cave_sample.vertical >= -0.80f && cave_sample.vertical <= cave_sample.height * 2.15f;
  if (!inside_cave_plan) {
    return {};
  }

  SurfaceSupportQuery cave_query = query;
  cave_query.max_above = std::max(cave_query.max_above, 0.42f);
  cave_query.max_below = std::max(cave_query.max_below, 4.20f);
  return cave_support_surfaces_.sample(cave_query);
}

TerrainSurfaceSample LumenRun::sampleWorldSupport(const SurfaceSupportQuery &query) const {
  const TerrainSurfaceSample world = support_surfaces_.sample(query);
  const TerrainSurfaceSample cave_floor = sampleCaveFloorSupport(query);
  if (!cave_floor.valid) {
    return world;
  }
  if (!world.valid || world.height > cave_floor.height + 0.28f) {
    return cave_floor;
  }
  return world;
}

void LumenRun::updateFishingVisual() {
  auto &objects = scene_.objects();
  if (fishing_line_objects_.empty() || fishing_float_object_ >= objects.size() ||
      pond_water_object_ >= objects.size()) {
    return;
  }

  RenderObject &water = objects[pond_water_object_];
  water.transform.position.y = 0.405f + std::sin(status_.elapsed_seconds * 1.4f) * 0.009f;
  if (inner_pond_water_object_ < objects.size()) {
    RenderObject &inner_water = objects[inner_pond_water_object_];
    inner_water.transform.position.y =
        0.430f + std::sin(status_.elapsed_seconds * 1.15f + 0.60f) * 0.012f;
  }

  const auto updateRig = [&](const std::vector<std::size_t> &line_objects,
                             const std::size_t float_object, const Vec3 rod_tip,
                             const Vec3 float_rest, const float phase_offset,
                             const float sag_base) {
    if (line_objects.empty() || float_object >= objects.size()) {
      return;
    }
    const Vec3 float_position =
        float_rest + Vec3{std::sin(status_.elapsed_seconds * 1.6f + phase_offset) * 0.026f,
                          std::sin(status_.elapsed_seconds * 2.2f + phase_offset) * 0.018f,
                          std::cos(status_.elapsed_seconds * 1.3f + phase_offset) * 0.020f};
    RenderObject &bobber = objects[float_object];
    bobber.transform.position = float_position;
    bobber.transform.rotation =
        quatFromEulerXyz({0.05f * std::sin(status_.elapsed_seconds * 1.7f + phase_offset),
                          status_.elapsed_seconds * 0.18f,
                          0.04f * std::cos(status_.elapsed_seconds * 1.3f + phase_offset)});

    const float sag = sag_base + 0.018f * std::sin(status_.elapsed_seconds * 1.1f + phase_offset);
    const float side_sway = std::sin(status_.elapsed_seconds * 0.8f + phase_offset) * 0.018f;
    Vec3 previous = rod_tip;
    for (std::size_t i = 0; i < line_objects.size(); ++i) {
      const float t1 = static_cast<float>(i + 1u) / static_cast<float>(line_objects.size());
      const Vec3 next = lineCurvePoint(rod_tip, float_position, t1, sag, side_sway);
      const Vec3 midpoint = (previous + next) * 0.5f;
      const float segment_length = std::max(length(next - previous), 0.035f);

      RenderObject &line = objects[line_objects[i]];
      line.transform.position = midpoint;
      line.transform.rotation = quatFromEulerXyz(segmentRotation(previous, next));
      line.transform.scale = {fishing_line_radius_, segment_length, fishing_line_radius_};
      previous = next;
    }
  };

  updateRig(fishing_line_objects_, fishing_float_object_, fishing_rod_tip_, fishing_float_rest_,
            0.0f, 0.12f);
  updateAquaticLifeVisual();
}

void LumenRun::updateAquaticLifeVisual() {
  auto &objects = scene_.objects();
  for (std::size_t i = 0; i < fish_.size(); ++i) {
    AquaticCreature &fish = fish_[i];
    if (fish.object_index >= objects.size()) {
      continue;
    }
    const float phase = fish.phase + status_.elapsed_seconds * fish.speed;
    const float wobble = std::sin(phase * 2.4f + static_cast<float>(i)) * 0.035f;
    const Vec3 position{fish.center.x + std::cos(phase) * fish.swim_radius.x,
                        fish.center.y + wobble,
                        fish.center.z + std::sin(phase * 0.86f) * fish.swim_radius.y};
    const Vec3 ahead{fish.center.x + std::cos(phase + 0.08f) * fish.swim_radius.x, fish.center.y,
                     fish.center.z + std::sin((phase + 0.08f) * 0.86f) * fish.swim_radius.y};
    RenderObject &object = objects[fish.object_index];
    object.transform.position = position;
    object.transform.rotation =
        quatFromEulerXyz({0.02f * std::sin(phase * 1.7f),
                          std::atan2(ahead.x - position.x, ahead.z - position.z),
                          0.10f * std::sin(phase * 2.1f)});
    object.transform.scale = fish.scale * (0.92f + 0.08f * std::sin(phase * 1.3f));
  }
}

void LumenRun::updateCastleBirds(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCastleBirds");
  if (castle_birds_.empty()) {
    return;
  }

  std::vector<AvianAgentState> flock;
  flock.reserve(castle_birds_.size());
  for (const CastleBird &bird : castle_birds_) {
    flock.push_back(bird.state);
  }

  for (CastleBird &bird : castle_birds_) {
    const AvianAgentUpdate update =
        updateAvianAgent(bird.state,
                         {.flight_center = castle_bird_flight_center_,
                          .flight_half_extents = castle_bird_flight_half_extents_,
                          .nest_position = castle_bird_nest_position_,
                          .max_speed = 1.75f + bird.state.temperament * 0.74f,
                          .max_force = 4.8f + bird.state.temperament * 1.7f,
                          .fear_radius = bird.fear_radius,
                          .nest_return_radius = 7.0f,
                          .landing_radius = 1.15f,
                          .perch_height = 0.32f,
                          .wander_strength = 0.46f + bird.state.temperament * 0.22f},
                         flock, player_position_, dt);
    bird.state.mode = update.mode;
  }
}

void LumenRun::updateCastleBirdVisuals() {
  auto &objects = scene_.objects();
  for (CastleBird &bird : castle_birds_) {
    if (bird.object_index >= objects.size()) {
      continue;
    }
    const float flap = std::sin(bird.state.wing_phase + bird.wing_phase_offset);
    const float cruise = 1.0f - bird.state.landing_blend;
    RenderObject &object = objects[bird.object_index];
    object.transform.position = bird.state.position;
    object.transform.rotation =
        quatFromEulerXyz({bird.state.pitch + flap * 0.055f * cruise, bird.state.facing_yaw,
                          bird.state.roll + flap * 0.16f * cruise});
    object.transform.scale =
        bird.scale * (0.96f + cruise * 0.04f * std::sin(bird.state.wing_phase * 0.57f));
    object.transform.scale.y *= 1.0f - bird.state.landing_blend * 0.12f;
  }
}

void LumenRun::updateCrocodile(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCrocodile");
  if (death_state_ != DeathSequenceState::Alive) {
    return;
  }
  const AmphibiousPredatorUpdate update =
      updateAmphibiousPredator(crocodile_state_,
                               {.water_center = inner_pond_center_,
                                .water_radius = inner_pond_radius_,
                                .water_surface_y = 0.430f,
                                .body_height = 0.16f,
                                .swim_submergence = 0.04f,
                                .swim_speed = 0.82f,
                                .shore_speed = 0.42f,
                                .pursue_speed = 1.08f,
                                .aggression = 0.54f,
                                .notice_radius = 4.65f,
                                .water_pursuit_margin = 0.26f,
                                .patrol_min_radius = 0.36f,
                                .patrol_max_radius = 0.84f,
                                .shore_radius = 1.0f,
                                .strike_radius = tuning_.player_radius + 0.34f,
                                .strike_cooldown = 1.85f},
                               player_position_, dt);
  crocodile_swim_blend_ = update.swim_blend;
  if (update.strike && invulnerability_ <= 0.0f) {
    triggerPlayerDeath(update.position);
  }
}

void LumenRun::updateCaveSkitters(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateCaveSkitters");
  if (death_state_ != DeathSequenceState::Alive || cave_skitters_.empty() || cave_webs_.empty()) {
    return;
  }

  const CaveWebObstacle *web = &cave_webs_.front();
  const Vec3 player_web_offset = player_position_ - web->center;
  const float player_plane_distance = std::abs(dot(player_web_offset, web->normal));
  const float player_web_x = dot(player_web_offset, web->side) / std::max(web->radius_x, 0.001f);
  const float player_web_y = dot(player_web_offset, web->up) / std::max(web->radius_y, 0.001f);
  const bool player_inside_web_volume =
      player_plane_distance <= std::max(web->thickness * 1.15f, 0.01f) &&
      player_web_x * player_web_x + player_web_y * player_web_y <= 1.0f;
  bool skitters_awake = web->broken || player_inside_web_volume;
  for (const CaveSkitter &skitter : cave_skitters_) {
    skitters_awake = skitters_awake || skitter.hit_flash > 0.0f || skitter.health < skitter.max_health;
  }

  std::vector<CaveSkitterAgentState> states;
  states.reserve(cave_skitters_.size());
  for (CaveSkitter &skitter : cave_skitters_) {
    skitter.state.dead = skitter.dead;
    states.push_back(skitter.state);
  }

  const std::vector<CaveSkitterAgentUpdate> updates = updateCaveSkitterGroup(
      states,
      {.web = {.center = web->center,
               .normal = web->normal,
               .side = web->side,
               .up = web->up,
               .radius_x = web->radius_x,
               .radius_y = web->radius_y,
               .thickness = web->thickness},
       .max_speed = 1.05f,
       .max_force = 5.4f,
       .patrol_speed_scale = 0.54f,
       .aggro_radius = skitters_awake ? 3.15f : 0.0f,
       .strike_radius = skitters_awake ? tuning_.player_radius + 0.34f : 0.0f,
       .bite_cooldown = 1.40f,
       .separation_radius = 0.34f,
       .cohesion_radius = 0.95f,
       .retreat_seconds = 0.36f,
       .surface_offset = -std::max(web->thickness * 0.64f, 0.12f),
       .depth_wander = std::max(web->thickness * 0.12f, 0.035f)},
      player_position_, dt);

  for (std::size_t i = 0; i < cave_skitters_.size(); ++i) {
    CaveSkitter &skitter = cave_skitters_[i];
    skitter.state = states[i];
    if (skitters_awake && i < updates.size() && updates[i].bite) {
      skitter.bite_flash = 1.0f;
      (void)applyPlayerDamage(kCaveSkitterBiteDamage, skitter.state.position);
    }
  }
}

void LumenRun::updateCrocodileVisual() {
  auto &objects = scene_.objects();
  if (crocodile_objects_.size() < kCrocodilePartCount) {
    return;
  }

  const Vec3 base = crocodile_state_.position;
  const float yaw = crocodile_state_.facing_yaw;
  const float swim = std::clamp(crocodile_swim_blend_, 0.0f, 1.0f);
  const float wave = std::sin(status_.elapsed_seconds * 4.1f + crocodile_state_.phase) * swim;
  const auto setPart = [&](const std::size_t part, const Vec3 offset, const Vec3 rotation,
                           const Vec3 scale) {
    if (part >= crocodile_objects_.size() || crocodile_objects_[part] >= objects.size()) {
      return;
    }
    RenderObject &object = objects[crocodile_objects_[part]];
    object.transform.position = base + rotateYaw(offset, yaw);
    object.transform.rotation = quatFromEulerXyz({rotation.x, yaw + rotation.y, rotation.z});
    object.transform.scale = scale;
  };

  setPart(0, {0.0f, 0.00f + wave * 0.018f, 0.0f}, {0.035f * wave, 0.0f, 0.020f * wave},
          {1.0f, 1.0f, 1.0f});
  const float jaw_open =
      crocodile_state_.mode == AmphibiousMotionMode::Strike ? 0.055f : 0.018f * swim;
  setPart(1, {0.0f, -0.064f - jaw_open, 0.86f}, {-0.14f - jaw_open * 3.2f, 0.0f, 0.0f},
          {0.17f, 0.030f, 0.30f});
  setPart(2, {-0.074f, 0.088f, 0.635f}, {0.0f, -0.035f, 0.0f}, {0.024f, 0.020f, 0.026f});
  setPart(3, {0.074f, 0.088f, 0.635f}, {0.0f, 0.035f, 0.0f}, {0.024f, 0.020f, 0.026f});
}

void LumenRun::updateCaveSkitterVisuals(const float dt) {
  auto &objects = scene_.objects();
  for (CaveSkitter &skitter : cave_skitters_) {
    if (skitter.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[skitter.object_index];
    if (skitter.dead || skitter.state.dead) {
      hideRenderObject(object);
      continue;
    }

    skitter.hit_flash = std::max(0.0f, skitter.hit_flash - dt * 4.8f);
    skitter.bite_flash = std::max(0.0f, skitter.bite_flash - dt * 5.6f);
    const float stride = length(skitter.state.velocity);
    const float gait = std::sin(skitter.state.phase * (8.0f + stride * 2.8f));
    const float alert =
        skitter.state.mode == CaveSkitterMotionMode::Pursue ||
                skitter.state.mode == CaveSkitterMotionMode::Strike
            ? 1.0f
            : 0.0f;
    const float damage = 1.0f - static_cast<float>(std::max(skitter.health, 0)) /
                                    static_cast<float>(std::max(skitter.max_health, 1));
    const Vec3 hit_recoil = skitter.last_hit_normal * (-skitter.hit_flash * 0.055f);
    object.transform.position =
        skitter.state.position + hit_recoil +
        Vec3{0.0f, 0.012f * gait - skitter.hit_flash * 0.010f, 0.0f};
    object.transform.rotation =
        quatFromEulerXyz({0.030f * gait,
                          skitter.state.facing_yaw,
                          0.040f * std::sin(skitter.state.phase * 5.4f) +
                              skitter.hit_flash * 0.080f});
    const float scale = 1.0f + skitter.bite_flash * 0.06f;
    object.transform.scale = {scale * (1.0f + skitter.hit_flash * 0.035f),
                              scale * (1.0f - damage * 0.05f - skitter.hit_flash * 0.12f),
                              scale * (1.0f + skitter.hit_flash * 0.080f)};
    object.material.emission_strength =
        0.020f + alert * 0.035f + skitter.hit_flash * 0.18f + skitter.bite_flash * 0.12f;
    object.material.edge_wear = 0.10f + damage * 0.24f;
    object.material.pattern_depth = 0.060f + damage * 0.055f + skitter.hit_flash * 0.035f;
    object.material.pattern_contrast = 0.88f + damage * 0.16f + skitter.hit_flash * 0.14f;
  }
}

void LumenRun::updateBloodParticles(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateBloodParticles");
  auto &objects = scene_.objects();
  for (SceneParticle &particle : blood_particles_) {
    if (particle.object_index >= objects.size()) {
      continue;
    }
    if (particle.age >= particle.lifetime) {
      objects[particle.object_index].transform.position = {0.0f, -20.0f, 0.0f};
      objects[particle.object_index].transform.scale = {0.001f, 0.001f, 0.001f};
      continue;
    }
    particle.age += dt;
    particle.velocity.y -= 5.6f * dt;
    particle.position = particle.position + particle.velocity * dt;
    const float life =
        std::clamp(1.0f - particle.age / std::max(particle.lifetime, 0.001f), 0.0f, 1.0f);
    RenderObject &object = objects[particle.object_index];
    object.transform.position = particle.position;
    object.transform.scale = {particle.size * (0.55f + life * 0.45f),
                              particle.size * (0.38f + life * 0.62f),
                              particle.size * (0.55f + life * 0.45f)};
    object.material.emission_strength = 0.02f + life * 0.08f;
  }
}

void LumenRun::updateMiningFractureVisuals(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateMiningFractureVisuals");
  auto &objects = scene_.objects();
  for (MiningFractureShardVisual &shard : mining_fracture_shards_) {
    if (shard.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[shard.object_index];
    if (!shard.active) {
      hideRenderObject(object);
      continue;
    }

    shard.age += std::max(dt, 0.0f);
    if (shard.age >= shard.lifetime) {
      shard.active = false;
      hideRenderObject(object);
      continue;
    }

    shard.velocity.y -= 3.9f * std::max(dt, 0.0f);
    shard.position = shard.position + shard.velocity * std::max(dt, 0.0f);
    shard.rotation = shard.rotation + shard.angular_velocity * std::max(dt, 0.0f);
    const float life =
        std::clamp(1.0f - shard.age / std::max(shard.lifetime, 0.001f), 0.0f, 1.0f);
    const float scale = shard.base_scale * (0.18f + 0.82f * life);
    object.transform.position = shard.position;
    object.transform.rotation = quatFromEulerXyz(shard.rotation);
    object.transform.scale = {scale, scale, scale};
    object.material.emission_strength = std::max(object.material.emission_strength, 0.035f * life);
  }
}

void LumenRun::spawnBloodBurst(const Vec3 center, const Vec3 impact_origin, const float intensity) {
  if (blood_particles_.empty()) {
    return;
  }
  const std::size_t count = std::clamp(
      static_cast<std::size_t>(std::ceil(std::max(intensity, 0.0f) * 8.0f)), std::size_t{3},
      blood_particles_.size());
  const Vec3 away_base = length(center - impact_origin) > 0.0001f
                             ? normalize(center - impact_origin)
                             : Vec3{0.0f, 0.45f, 1.0f};
  for (std::size_t emitted = 0; emitted < count; ++emitted) {
    const std::size_t particle_index = (blood_particle_cursor_ + emitted) % blood_particles_.size();
    SceneParticle &particle = blood_particles_[particle_index];
    const float serial = static_cast<float>(blood_particle_cursor_ + emitted);
    const float angle = serial * (kPi * (3.0f - std::sqrt(5.0f)));
    const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
    const float ring = 0.22f + 0.32f * (static_cast<float>(emitted % 5u) / 4.0f);
    particle.position = center + Vec3{0.0f, 0.08f, 0.0f};
    particle.velocity = radial * ring + away_base * (0.20f + 0.10f * intensity) +
                        Vec3{0.0f, 0.52f + 0.28f * std::sin(angle * 1.7f), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.42f + 0.18f * (static_cast<float>(emitted % 4u) / 3.0f);
    particle.size = 0.016f + 0.014f * std::clamp(intensity, 0.0f, 1.5f);
  }
  blood_particle_cursor_ += count;
}

void LumenRun::clearTransientFeedback() {
  player_mouth_open_ = 0.0f;
  death_state_ = DeathSequenceState::Alive;
  death_timer_ = 0.0f;
  death_origin_ = {};
  pending_defeat_ = false;
  blood_particle_cursor_ = 0;

  auto &objects = scene_.objects();
  for (SceneParticle &particle : blood_particles_) {
    particle.position = {0.0f, -20.0f, 0.0f};
    particle.velocity = {};
    particle.age = 0.0f;
    particle.lifetime = 0.0f;
    if (particle.object_index < objects.size()) {
      hideRenderObject(objects[particle.object_index]);
    }
  }
  for (MiningFractureShardVisual &shard : mining_fracture_shards_) {
    shard.active = false;
    shard.age = shard.lifetime;
    shard.velocity = {};
    shard.angular_velocity = {};
    if (shard.object_index < objects.size()) {
      hideRenderObject(objects[shard.object_index]);
    }
  }
  for (CoalOreNode &ore : coal_ores_) {
    ore.hit_flash = 0.0f;
  }
  for (CaveWebObstacle &web : cave_webs_) {
    web.hit_flash = 0.0f;
  }
  for (CaveSkitter &skitter : cave_skitters_) {
    skitter.hit_flash = 0.0f;
    skitter.bite_flash = 0.0f;
    skitter.last_hit_normal = {0.0f, 1.0f, 0.0f};
  }
  mining_.reset();
  restorePlayerEyeObjects();
}

void LumenRun::triggerPlayerDeath(const Vec3 impact_origin) {
  if (death_state_ != DeathSequenceState::Alive) {
    return;
  }
  status_.health = 0;
  --status_.lives;
  pending_defeat_ = status_.lives <= 0;
  invulnerability_ = tuning_.invulnerability_seconds + 1.2f;
  death_state_ = DeathSequenceState::Falling;
  death_timer_ = 0.0f;
  death_origin_ = player_position_;
  player_velocity_ = {};
  player_mouth_open_ = 1.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  clearAvatarPointTarget();
  if (physics_.valid(player_body_)) {
    physics_.setVelocity(player_body_, {});
  }

  for (std::size_t i = 0; i < blood_particles_.size(); ++i) {
    SceneParticle &particle = blood_particles_[i];
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const float ring = 0.35f + 0.45f * (static_cast<float>(i % 7u) / 6.0f);
    const Vec3 away = normalizeOr(player_position_ - impact_origin, Vec3{0.0f, 0.45f, 1.0f});
    const Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
    particle.position = player_position_ + Vec3{0.0f, 0.16f, 0.0f};
    particle.velocity =
        radial * ring + away * 0.42f + Vec3{0.0f, 1.2f + 0.35f * std::sin(angle * 1.7f), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.75f + 0.28f * (static_cast<float>(i % 5u) / 4.0f);
    particle.size = 0.025f + 0.018f * (static_cast<float>(i % 4u) / 3.0f);
  }
}

bool LumenRun::applyPlayerDamage(const int hit_points, const Vec3 impact_origin) {
  if (hit_points <= 0 || invulnerability_ > 0.0f || death_state_ != DeathSequenceState::Alive ||
      status_.defeated) {
    return false;
  }
  status_.max_health = std::max(status_.max_health, kPlayerMaxHealth);
  status_.health = std::clamp(status_.health - hit_points, 0, status_.max_health);
  if (status_.health <= 0) {
    triggerPlayerDeath(impact_origin);
    return true;
  }
  invulnerability_ = std::max(invulnerability_, 0.62f);
  player_mouth_open_ = 1.0f;
  for (std::size_t i = 0; i < blood_particles_.size(); i += 3u) {
    SceneParticle &particle = blood_particles_[i];
    const float angle = static_cast<float>(i) * (kPi * (3.0f - std::sqrt(5.0f)));
    const Vec3 away = length(player_position_ - impact_origin) > 0.0001f
                          ? normalize(player_position_ - impact_origin)
                          : Vec3{std::cos(angle), 0.0f, std::sin(angle)};
    particle.position = player_position_ + Vec3{0.0f, 0.18f, 0.0f};
    particle.velocity = away * (0.36f + 0.10f * static_cast<float>(i % 4u)) +
                        Vec3{0.0f, 0.62f + 0.08f * std::sin(angle), 0.0f};
    particle.age = 0.0f;
    particle.lifetime = 0.42f + 0.08f * static_cast<float>(i % 3u);
    particle.size = 0.018f + 0.006f * static_cast<float>(i % 2u);
  }
  return true;
}

void LumenRun::updateDeathSequence(const float dt) {
  death_timer_ += dt;
  const float fall_duration = 0.82f;
  if (death_state_ == DeathSequenceState::Falling && death_timer_ >= fall_duration) {
    death_state_ = DeathSequenceState::Blinking;
    death_timer_ = 0.0f;
    return;
  }
  if (death_state_ == DeathSequenceState::Blinking && death_timer_ >= 1.15f) {
    if (pending_defeat_) {
      status_.defeated = true;
      death_state_ = DeathSequenceState::Alive;
      return;
    }
    respawnPlayer();
  }
}

void LumenRun::updateDeathVisuals() {
  auto &objects = scene_.objects();
  const float fall_t = death_state_ == DeathSequenceState::Falling
                           ? std::clamp(death_timer_ / 0.82f, 0.0f, 1.0f)
                           : 1.0f;

  for (const std::size_t object_index : player_avatar_instance_.object_indices) {
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[object_index];
    object.transform.position.y -= fall_t * 0.22f;
    object.transform.rotation =
        quatFromEulerXyz(eulerXyz(object.transform.rotation) +
                         Vec3{fall_t * 1.42f, 0.0f, fall_t * 0.62f});
  }

  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < objects.size()) {
      objects[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }

  if (!eye_objects_valid_ || left_eye_object_ >= objects.size() ||
      right_eye_object_ >= objects.size()) {
    return;
  }

  const auto apply_eye_cross = [&](const std::size_t eye_index, const std::size_t overlay_slot) {
    if (overlay_slot >= x_eye_objects_.size() || x_eye_objects_[overlay_slot] >= objects.size()) {
      return;
    }
    const RenderObject &eye = objects[eye_index];
    RenderObject &overlay = objects[x_eye_objects_[overlay_slot]];
    const Vec3 forward = rotate(eye.transform.rotation, {0.0f, 0.0f, 1.0f});
    const float radius = std::max({std::abs(eye.transform.scale.x), std::abs(eye.transform.scale.y),
                                   std::abs(eye.transform.scale.z)});
    overlay.transform.position = eye.transform.position + forward * (radius * 1.08f + 0.006f);
    overlay.transform.rotation = eye.transform.rotation;
    overlay.transform.scale = {std::max(radius * 3.35f, 0.044f), std::max(radius * 3.35f, 0.044f),
                               0.010f};
    overlay.camera_occlusion_fade = false;
  };
  apply_eye_cross(left_eye_object_, 0u);
  apply_eye_cross(right_eye_object_, 1u);
}

void LumenRun::restorePlayerEyeObjects() {
  auto &objects = scene_.objects();
  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < objects.size()) {
      objects[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }
  if (!eye_objects_valid_) {
    return;
  }

  const auto restore_eye = [&](const std::size_t object_index) {
    if (object_index >= objects.size()) {
      return;
    }
    for (std::size_t i = 0;
         i < player_avatar_.parts.size() && i < player_avatar_instance_.object_indices.size();
         ++i) {
      if (player_avatar_instance_.object_indices[i] != object_index) {
        continue;
      }
      const AvatarPart &part = player_avatar_.parts[i];
      RenderObject &eye = objects[object_index];
      eye.primitive = part.primitive;
      eye.custom_mesh.reset();
      eye.material = part.material;
      eye.camera_occlusion_fade = false;
      return;
    }
  };
  restore_eye(left_eye_object_);
  restore_eye(right_eye_object_);
}

void LumenRun::respawnPlayer() {
  const TerrainSurfaceSample start_ground =
      sampleWorldSupport({{0.0f, 0.0f}, playerSupportExtent(), 0.30f, 2.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  player_velocity_ = {};
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_mouth_open_ = 0.0f;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  status_.health = status_.max_health;
  clearAvatarPointTarget();
  clearTransientFeedback();
  forced_spawn_lighting_frames_ = 2;
  invulnerability_ = tuning_.invulnerability_seconds;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  for (const std::size_t object_index : x_eye_objects_) {
    if (object_index < scene_.objects().size()) {
      scene_.objects()[object_index].transform.position = {0.0f, -20.0f, 0.0f};
    }
  }
}

void LumenRun::enforceWorldBounds() {
  const Vec2 planar{player_position_.x, player_position_.z};
  const bool invalid_position = !std::isfinite(player_position_.x) ||
                                !std::isfinite(player_position_.y) ||
                                !std::isfinite(player_position_.z);
  CaveInteriorSample cave_sample{};
  const bool inside_authored_cave =
      !invalid_position && caveSectionAt(player_position_, &cave_sample) != nullptr &&
      cave_sample.interior > 0.08f;
  const bool outside_radius =
      !inside_authored_cave &&
      length(planar) > std::max(tuning_.playable_radius, tuning_.arena_radius * 2.0f);
  const bool below_recovery_floor =
      !inside_authored_cave && player_position_.y < tuning_.recovery_floor_y;
  if (!invalid_position && !outside_radius && !below_recovery_floor) {
    return;
  }

  const TerrainSurfaceSample start_ground =
      sampleWorldSupport({{0.0f, 0.0f}, playerSupportExtent(), 0.30f, 2.0f});
  player_position_ = {
      0.0f, (start_ground.valid ? start_ground.height : 0.0f) + playerSupportExtent(), 0.0f};
  player_velocity_ = {};
  player_render_position_ = player_position_;
  player_render_position_valid_ = false;
  player_avatar_pose_valid_ = false;
  player_swimming_ = false;
  player_swim_blend_ = 0.0f;
  player_climbing_ = false;
  player_climb_blend_ = 0.0f;
  clearAvatarPointTarget();
  invulnerability_ = std::max(invulnerability_, 0.35f);
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
}

bool LumenRun::isSwimmableWater(const Vec3 support_position) const {
  const float support_ceiling = playerSupportExtent() * 1.20f;
  const auto in_water = [&](const Vec3 center, const Vec2 radius, const float surface_y) {
    if (support_position.y > surface_y + support_ceiling) {
      return false;
    }
    return insideEllipticalFootprint(support_position, center, radius, 0.92f);
  };

  return in_water(pond_center_, kCourtyardPondRadius, kCourtyardPondSurfaceY) ||
         in_water(inner_pond_center_, inner_pond_radius_, kInnerPondSurfaceY);
}

void LumenRun::collectOverlaps() {
  ASTER_PROFILE_SCOPE("LumenRun::collectOverlaps");
  for (Shard &shard : shards_) {
    if (shard.collected) {
      continue;
    }
    if (distanceOnArena(player_position_, shard.position) <=
        tuning_.player_radius + tuning_.shard_radius) {
      shard.collected = true;
      ++status_.score;
    }
  }
  status_.victory = status_.score >= status_.total_shards;
}

void LumenRun::resolveSentinelImpacts() {
  ASTER_PROFILE_SCOPE("LumenRun::resolveSentinelImpacts");
  if (invulnerability_ > 0.0f) {
    return;
  }

  const auto &objects = scene_.objects();
  const VerticalCapsuleContactVolume player_volume{
      player_position_,
      tuning_.player_radius,
      std::max(tuning_.player_height * 0.5f - tuning_.player_radius, 0.0f),
  };
  for (const Sentinel &sentinel : sentinels_) {
    const Vec3 sentinel_position = objects[sentinel.object_index].transform.position;
    const VerticalCapsuleContactVolume sentinel_volume{
        sentinel_position,
        tuning_.sentinel_radius * 0.82f,
        tuning_.sentinel_radius * 1.30f,
    };
    if (overlaps(player_volume, sentinel_volume)) {
      triggerPlayerDeath(sentinel_position);
      return;
    }
  }
}

float LumenRun::playerSupportExtent() const {
  return tuning_.player_height * 0.5f;
}

Vec3 LumenRun::avatarPosePosition() const {
  return avatarPosePosition(player_position_);
}

Vec3 LumenRun::avatarPosePosition(const Vec3 player_position) const {
  const float visual_offset = std::max(player_avatar_support_extent_ - playerSupportExtent(), 0.0f);
  return player_position + Vec3{0.0f, visual_offset, 0.0f};
}

} // namespace aster
