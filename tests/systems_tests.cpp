// Author: Faruk Alpay
// Do not remove this notice.

#include "test_support.hpp"

namespace {

void testGameplayItemInteractionSystems() {
  aster::ItemRegistry registry;
  registry.add({.id = "torch",
                .display_name = "Torch",
                .short_label = "TRC",
                .type = aster::ItemType::LightTool,
                .tint = {1.0f, 0.45f, 0.12f},
                .creates_light = true,
                .creates_fire_particles = true});
  registry.add({.id = "pickaxe",
                .display_name = "Pickaxe",
                .short_label = "PCK",
                .type = aster::ItemType::Tool,
                .tint = {0.5f, 0.48f, 0.42f}});
  assert(registry.contains("torch"));

  aster::InventoryContainer chest(2u);
  assert(chest.addItem(*registry.find("torch"), 1).has_value());
  assert(chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(!chest.addItem(*registry.find("pickaxe"), 1).has_value());
  assert(chest.removeItem("torch", 1));

  aster::Hotbar hotbar(3u);
  const std::optional<std::size_t> slot = hotbar.addItem(*registry.find("torch"), 1);
  assert(slot.has_value());
  assert(hotbar.select(*slot));
  aster::EquipmentSystem equipment;
  equipment.equipFromHotbar(hotbar);
  assert(equipment.isEquipped("torch"));

  aster::InteractionSystem interactions;
  interactions.update({{.id = "item:torch",
                        .kind = aster::InteractionTargetKind::Item,
                        .action_label = "Take",
                        .subject_label = "Torch",
                        .position = {0.0f, 0.0f, 3.0f},
                        .radius = 0.35f,
                        .max_distance = 6.0f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:torch");
  interactions.clear();
  interactions.update({{.id = "item:nearby",
                        .kind = aster::InteractionTargetKind::Item,
                        .action_label = "Use",
                        .subject_label = "Nearby",
                        .position = {2.0f, 0.0f, 0.0f},
                        .radius = 0.10f,
                        .max_distance = 6.0f,
                        .proximity_distance = 2.25f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {2.0f, 0.0f, 0.0f},
                      1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:nearby");
  interactions.clear();
  interactions.update({{.id = "item:occluded",
                        .kind = aster::InteractionTargetKind::Item,
                        .shape = aster::InteractionTargetShape::ExplicitHit,
                        .action_label = "Use",
                        .subject_label = "Occluded",
                        .position = {0.0f, 0.0f, 1.0f},
                        .max_distance = 6.0f,
                        .hit_distance = 1.0f,
                        .evidence_strength = 4.0f,
                        .occluded = true,
                        .enabled = true},
                       {.id = "item:visible",
                        .kind = aster::InteractionTargetKind::Item,
                        .shape = aster::InteractionTargetShape::ExplicitHit,
                        .action_label = "Use",
                        .subject_label = "Visible",
                        .position = {0.0f, 0.0f, 1.2f},
                        .max_distance = 6.0f,
                        .hit_distance = 1.2f,
                        .evidence_strength = 1.0f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:visible");
  interactions.update({{.id = "item:weak",
                        .kind = aster::InteractionTargetKind::Item,
                        .shape = aster::InteractionTargetShape::ExplicitHit,
                        .action_label = "Use",
                        .subject_label = "Weak",
                        .position = {0.0f, 0.0f, 1.0f},
                        .max_distance = 6.0f,
                        .hit_distance = 1.0f,
                        .evidence_strength = 1.0f,
                        .enabled = true},
                       {.id = "item:strong",
                        .kind = aster::InteractionTargetKind::Item,
                        .shape = aster::InteractionTargetShape::ExplicitHit,
                        .action_label = "Use",
                        .subject_label = "Strong",
                        .position = {0.0f, 0.0f, 1.01f},
                        .max_distance = 6.0f,
                        .hit_distance = 1.01f,
                        .evidence_strength = 3.0f,
                        .enabled = true}},
                      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.0f / 60.0f);
  assert(interactions.focus().visible);
  assert(interactions.focus().target_id == "item:strong");

  aster::ScalarAnimation animation;
  animation.setTarget(1.0f);
  animation.update(1.0f / 15.0f);
  assert(animation.value() > 0.0f);
  aster::FlickerLightSpec light_spec;
  light_spec.source_radius = 0.70f;
  const aster::DynamicPointLight light =
      aster::evaluateFlickerLight(light_spec, {1.0f, 2.0f, 3.0f}, 0.25f);
  assert(light.active);
  assert(light.intensity > 0.0f);
  expectNear(light.source_radius, 0.70f, 0.0001f);

  aster::ParticleEmitter emitter(3u);
  emitter.update(1.0f / 60.0f, {0.0f, 0.0f, 0.0f}, 0.5f, {.max_particles = 3u});
  assert(emitter.particles().size() == 3u);
  assert(emitter.particles().front().active);

  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 1.0f});
  aster::AvatarPose pose;
  pose.position = {1.0f, 2.0f, 3.0f};
  pose.facing_yaw = aster::radians(35.0f);
  pose.gait_phase = 0.8f;
  pose.stride_amplitude = 0.35f;
  const std::optional<aster::Transform> socket = aster::resolveAvatarAttachmentSocket(
      rig, pose, {.part_name = "right paw", .local_position = {0.0f, -0.10f, 0.20f}});
  assert(socket.has_value());
  assert(aster::length(socket->position - pose.position) > 0.10f);
  assert(!aster::resolveAvatarAttachmentSocket(rig, pose, {.part_name = "missing"}).has_value());
}

void testMiningDamageAccumulatesAcrossOneCutFootprint() {
  aster::MiningState mining;
  aster::MiningToolStats tool = aster::starterPickaxeStats();
  tool.cooldown_seconds = 0.0f;
  const float hardness = 3.25f;

  aster::VoxelCaveHit hit;
  hit.hit = true;
  hit.material = aster::VoxelCaveMaterial::Rock;
  hit.normal = {0.0f, 0.0f, 1.0f};
  hit.point = {0.0f, 0.0f, 0.0f};
  hit.cell = {10, 0, 0};

  aster::MiningFeedback feedback;
  for (int strike = 0; strike < 4; ++strike) {
    hit.point = {static_cast<float>(strike) * tool.carve_radius * 0.18f, 0.0f, 0.0f};
    hit.cell = {10 + strike, 0, 0};
    feedback = mining.tryMine({.now_seconds = static_cast<float>(strike),
                               .hit = hit,
                               .tool = tool,
                               .material_hardness = hardness});
    assert(feedback.accepted);
    assert(feedback.crack_fraction > 0.0f);
    assert(feedback.carved == (strike == 3));
  }
}

void testGenericMineableBreaksWithoutVoxelCarve() {
  aster::MiningState mining;
  aster::MiningToolStats tool = aster::starterPickaxeStats();
  tool.cooldown_seconds = 0.0f;
  tool.power = 1.0f;

  const aster::MineableHit web_hit{.hit = true,
                                   .target_key = "lumen.web.test",
                                   .point = {0.0f, 1.0f, -2.0f},
                                   .normal = {0.0f, 0.0f, 1.0f},
                                   .material = aster::VoxelCaveMaterial::Rock,
                                   .cell = {3, 4, 5}};
  aster::MiningFeedback feedback;
  for (int strike = 0; strike < 2; ++strike) {
    const aster::MineableAttempt attempt{.now_seconds = static_cast<float>(strike),
                                         .hit = web_hit,
                                         .tool = tool,
                                         .material_hardness = 2.0f,
                                         .resource_item_id = {},
                                         .resource_quantity = 0,
                                         .carve_surface = false};
    feedback = mining.tryMine(attempt);
    assert(feedback.accepted);
    assert(feedback.crack_fraction > 0.0f);
    assert(feedback.carved == (strike == 1));
  }
  assert(feedback.edit.radius == 0.55f);
  bool saw_carve_event = false;
  for (const aster::VoxelImpactEvent &event : feedback.impact_events) {
    saw_carve_event = saw_carve_event || event.kind == aster::VoxelImpactEventKind::Carve;
  }
  assert(!saw_carve_event);
}

void testControlScheme() {
  aster::ControlScheme controls;
  controls.addCommand("camera.orbit.left");
  controls.bind("camera.orbit.left", {aster::ControlDevice::Keyboard, 263});

  aster::ControlState state;
  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(state.justPressed("camera.orbit.left"));

  state.update(controls, {{263}, {}, {}});
  assert(state.pressed("camera.orbit.left"));
  assert(!state.justPressed("camera.orbit.left"));

  state.update(controls, {{}, {}, {}});
  assert(!state.pressed("camera.orbit.left"));
  assert(state.justReleased("camera.orbit.left"));
}

void testPlayerMotionPlan() {
  const aster::PlayerMovePlan plan = aster::buildPlayerMovePlan(
      {.walk_speed = 2.0f, .run_multiplier = 1.5f, .response_rate = 10.0f, .jump_speed = 5.0f},
      {{2.0f, 0.0f}, true, true});
  expectNear(plan.target_speed, 3.0f, 0.0001f);
  expectNear(aster::length({plan.input.desired_velocity.x, 0.0f, plan.input.desired_velocity.z}),
             3.0f, 0.0001f);
  assert(plan.input.jump_requested);
  expectNear(plan.character_settings.jump_speed, 5.0f, 0.0001f);
}

void testSwimMotionPlan() {
  const aster::PhysicsFluidSample fluid{true, 0.75f, 0.40f, 0.24f, {0.20f, 0.0f, 0.0f}};
  const aster::SwimMotionResult swim =
      aster::buildSwimMotion(fluid, {0.0f, -1.0f, 0.0f}, {{2.0f, 0.0f, 0.0f}, true},
                             {.activation_submersion = 0.25f,
                              .full_swim_submersion = 0.75f,
                              .horizontal_speed_scale = 0.5f,
                              .flow_influence = 0.5f,
                              .surface_clearance = 0.10f,
                              .float_response = 4.0f,
                              .max_upward_speed = 1.0f,
                              .max_downward_speed = 0.4f,
                              .ascend_speed = 1.2f});
  assert(swim.swimming);
  expectNear(swim.blend, 1.0f, 0.0001f);
  assert(swim.desired_velocity.x > 1.0f);
  assert(swim.target_vertical_velocity >= 1.2f);
}

void testClimbMotionPlan() {
  const aster::ClimbableCylinder tree{{0.0f, 0.0f, 0.0f}, 0.40f, 3.0f};
  const aster::ClimbSurfaceSample sample =
      aster::sampleClimbableCylinder(tree, {0.55f, 1.2f, 0.0f},
                                     {.capture_distance = 0.22f,
                                      .character_clearance = 0.16f,
                                      .stick_response = 10.0f,
                                      .tangent_speed_scale = 0.5f,
                                      .ascend_speed = 1.4f,
                                      .hold_vertical_speed = 0.1f,
                                      .max_correction = 0.12f});
  assert(sample.climbable);
  const aster::ClimbMotionResult climb = aster::buildClimbMotion(
      sample,
      {.desired_velocity = {0.0f, 0.0f, 1.0f}, .engage_requested = true, .ascend_requested = true},
      {.capture_distance = 0.22f,
       .character_clearance = 0.16f,
       .stick_response = 10.0f,
       .tangent_speed_scale = 0.5f,
       .ascend_speed = 1.4f,
       .hold_vertical_speed = 0.1f,
       .max_correction = 0.12f});
  assert(climb.climbing);
  assert(climb.desired_velocity.y >= 1.4f);
  assert(aster::length(climb.position_correction) > 0.0f);
}

void testAmphibiousPredatorMotion() {
  aster::AmphibiousPredatorState state;
  state.position = {-0.4f, 0.0f, 0.0f};
  const aster::AmphibiousPredatorUpdate update =
      aster::updateAmphibiousPredator(state,
                                      {.water_center = {0.0f, 0.0f, 0.0f},
                                       .water_radius = {2.0f, 1.0f},
                                       .water_surface_y = 0.2f,
                                       .body_height = 0.2f,
                                       .swim_speed = 0.5f,
                                       .shore_speed = 0.3f,
                                       .pursue_speed = 1.0f,
                                       .aggression = 0.5f,
                                       .notice_radius = 3.0f,
                                       .water_pursuit_margin = 0.1f,
                                       .strike_radius = 0.5f,
                                       .strike_cooldown = 1.0f},
                                      {0.0f, 0.2f, 0.0f}, 0.2f);
  assert(update.mode == aster::AmphibiousMotionMode::Pursue ||
         update.mode == aster::AmphibiousMotionMode::Strike);
  assert(aster::length(update.position - aster::Vec3{-0.4f, 0.0f, 0.0f}) > 0.0f);
}

void testCaveSkitterGroupPatrolsAndBitesInsideWeb() {
  std::vector<aster::CaveSkitterAgentState> skitters(3);
  skitters[0].home_offset = {-0.30f, -0.10f};
  skitters[1].home_offset = {0.22f, 0.16f};
  skitters[2].home_offset = {0.04f, -0.22f};
  for (std::size_t i = 0; i < skitters.size(); ++i) {
    skitters[i].position = {skitters[i].home_offset.x, skitters[i].home_offset.y, 0.0f};
    skitters[i].temperament = 0.2f + static_cast<float>(i) * 0.2f;
  }
  const aster::CaveSkitterGroupSettings settings{
      .web = {.center = {0.0f, 0.0f, 0.0f},
              .normal = {0.0f, 0.0f, 1.0f},
              .side = {1.0f, 0.0f, 0.0f},
              .up = {0.0f, 1.0f, 0.0f},
              .radius_x = 1.2f,
              .radius_y = 0.8f,
              .thickness = 0.30f},
      .max_speed = 1.1f,
      .max_force = 5.0f,
      .aggro_radius = 2.5f,
      .strike_radius = 0.35f,
      .bite_cooldown = 1.4f};
  bool saw_bite = false;
  for (int step = 0; step < 18; ++step) {
    const std::vector<aster::CaveSkitterAgentUpdate> updates =
        aster::updateCaveSkitterGroup(skitters, settings, {0.02f, 0.02f, 0.0f}, 1.0f / 30.0f);
    assert(updates.size() == skitters.size());
    for (const aster::CaveSkitterAgentState &skitter : skitters) {
      const float x = skitter.position.x / settings.web.radius_x;
      const float y = skitter.position.y / settings.web.radius_y;
      assert(x * x + y * y <= 1.05f);
      assert(std::abs(skitter.position.z) <= settings.web.thickness * 0.55f);
    }
    saw_bite = saw_bite || updates[0].bite || updates[1].bite || updates[2].bite;
  }
  assert(saw_bite);
  const float cooldown_after_bite = skitters.front().bite_cooldown;
  (void)aster::updateCaveSkitterGroup(skitters, settings, {0.02f, 0.02f, 0.0f}, 1.0f / 60.0f);
  assert(skitters.front().bite_cooldown <= cooldown_after_bite);

  aster::CaveSkitterGroupSettings dormant_settings = settings;
  dormant_settings.aggro_radius = 0.0f;
  dormant_settings.strike_radius = 0.0f;
  for (aster::CaveSkitterAgentState &skitter : skitters) {
    skitter.bite_cooldown = 0.0f;
  }
  const std::vector<aster::CaveSkitterAgentUpdate> dormant_updates =
      aster::updateCaveSkitterGroup(skitters, dormant_settings, {0.02f, 0.02f, 0.0f},
                                    1.0f / 30.0f);
  for (const aster::CaveSkitterAgentUpdate &update : dormant_updates) {
    assert(!update.bite);
  }
}

void testAvatarRigSceneBinding() {
  aster::Material fur;
  fur.surface_pattern = aster::SurfacePattern::FurFibers;
  aster::Material face;
  const aster::AvatarRig rig = aster::makePlushHumanoidAvatar({.height = 0.7f,
                                                               .fur_material = fur,
                                                               .muzzle_material = face,
                                                               .eye_material = face,
                                                               .nose_material = face});
  assert(rig.parts.size() >= 12u);
  bool has_pointing_finger = false;
  for (const aster::AvatarPart &part : rig.parts) {
    has_pointing_finger = has_pointing_finger || part.role == aster::AvatarPartRole::PointingFinger;
  }
  assert(has_pointing_finger);
  const aster::AvatarBounds bounds = aster::avatarLocalBounds(rig);
  assert(bounds.min.y < -0.35f);
  assert(aster::avatarGroundSupportExtent(rig) > 0.35f);

  aster::Scene scene;
  const aster::AvatarInstance instance =
      aster::appendAvatar(scene, rig, {.position = {1.0f, 0.5f, -2.0f}});
  assert(instance.object_indices.size() == rig.parts.size());
  assert(scene.objects().size() == rig.parts.size());
  aster::applyAvatarPose(scene, rig, instance,
                         {.position = {1.0f, 0.5f, -2.0f},
                          .facing_yaw = aster::radians(45.0f),
                          .gait_phase = 1.0f,
                          .stride_amplitude = 0.5f,
                          .vertical_bob = 0.01f,
                          .head_yaw_offset = 0.35f,
                          .mouth_open = 1.0f});
  assert(scene.objects().front().material.surface_pattern == aster::SurfacePattern::FurFibers);
  assert(scene.objects().front().transform.position.y > 0.0f);
  bool saw_head_turn = false;
  bool saw_open_mouth = false;
  for (std::size_t i = 0; i < rig.parts.size(); ++i) {
    if (rig.parts[i].joint == aster::AvatarJoint::Head) {
      const std::size_t object_index = instance.object_indices[i];
      saw_head_turn =
          saw_head_turn || aster::eulerXyz(scene.objects()[object_index].transform.rotation).y > 1.0f;
    }
    if (rig.parts[i].joint == aster::AvatarJoint::Mouth) {
      const std::size_t object_index = instance.object_indices[i];
      saw_open_mouth = scene.objects()[object_index].transform.scale.y >
                       rig.parts[i].local_transform.scale.y * 3.0f;
    }
  }
  assert(saw_head_turn);
  assert(saw_open_mouth);

  aster::AvatarAnimatorState animator;
  const aster::AvatarPose pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .head_yaw_offset = aster::radians(30.0f),
                                   .mouth_open = 1.0f},
                                  1.0f / 60.0f);
  assert(animator.initialized);
  assert(pose.stride_amplitude > 0.0f);
  assert(pose.head_yaw_offset > 0.0f);
  assert(pose.mouth_open > 0.0f);
  const aster::AvatarPose point_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {},
                                   .desired_facing_yaw = 0.0f,
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .has_attention_target = true,
                                   .attention_target = {1.0f, 1.1f, 2.0f},
                                   .pointing_enabled = true},
                                  1.0f / 30.0f);
  assert(point_pose.point_blend > 0.0f);
  assert(std::abs(point_pose.point_yaw_offset) > 0.001f);
  const aster::AvatarPose swim_pose =
      aster::updateAvatarAnimator(animator, {},
                                  {.position = {0.0f, 1.0f, 0.0f},
                                   .velocity = {1.0f, 0.0f, 0.0f},
                                   .desired_facing_yaw = aster::radians(90.0f),
                                   .has_facing_target = true,
                                   .max_planar_speed = 3.0f,
                                   .swim_blend = 1.0f},
                                  1.0f / 30.0f);
  assert(swim_pose.swim_blend > 0.0f);
}

void testThirdPersonFollowController() {
  aster::ThirdPersonFollowState state;
  const aster::ThirdPersonFollowPose pose =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = true,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {40.0f, -12.0f},
                                      .focus_target = {0.0f, 1.0f, 0.0f},
                                      .fallback_yaw = aster::radians(38.0f),
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(pose.active);
  assert(std::abs(pose.camera_yaw - aster::radians(38.0f)) > 0.001f);
  assert(pose.camera_pitch > aster::radians(28.0f));
  expectNear(pose.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose shifted =
      aster::updateThirdPersonFollow(state, {.target_response = 12.0f},
                                     {.active = true,
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(shifted.camera_target.x > 0.0f);
  assert(shifted.camera_target.x < 10.0f);
  const aster::ThirdPersonFollowPose snapped = aster::updateThirdPersonFollow(
      state, {.target_response = 12.0f, .teleport_snap_distance = 4.0f},
      {.active = true,
       .focus_target = {40.0f, 1.0f, 0.0f},
       .fallback_yaw = 0.0f,
       .fallback_pitch = aster::radians(28.0f)},
      1.0f / 60.0f);
  expectNear(snapped.camera_target.x, 40.0f, 0.0001f);
  expectNear(snapped.camera_target.y, 1.0f, 0.0001f);
  const aster::ThirdPersonFollowPose released =
      aster::updateThirdPersonFollow(state, {},
                                     {.active = false,
                                      .has_pointer_delta = true,
                                      .pointer_delta = {100.0f, 0.0f},
                                      .focus_target = {10.0f, 1.0f, 0.0f},
                                      .fallback_yaw = 0.0f,
                                      .fallback_pitch = aster::radians(28.0f)},
                                     1.0f / 60.0f);
  assert(!released.active);
  expectNear(released.camera_yaw, snapped.camera_yaw, 0.0001f);

  const aster::Vec2 forward = aster::cameraRelativeMoveAxis({0.0f, 1.0f}, 0.0f);
  expectNear(forward.x, 0.0f, 0.0001f);
  expectNear(forward.y, -1.0f, 0.0001f);
  const aster::Vec2 rotated_forward =
      aster::cameraRelativeMoveAxis({0.0f, 1.0f}, aster::radians(90.0f));
  expectNear(rotated_forward.x, -1.0f, 0.0001f);
  expectNear(rotated_forward.y, 0.0f, 0.0001f);

  aster::OrbitCamera camera;
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.yaw = 0.0f;
  camera.pitch = 0.0f;
  camera.radius = 4.0f;
  const aster::CameraRay ray =
      camera.screenRay({400.0f, 300.0f, 0.0f}, aster::Viewport{{}, {800.0f, 600.0f}});
  assert(aster::length(ray.direction) > 0.99f);
  assert(ray.direction.z < -0.99f);
}

} // namespace

int main() {
  testGameplayItemInteractionSystems();
  testMiningDamageAccumulatesAcrossOneCutFootprint();
  testGenericMineableBreaksWithoutVoxelCarve();
  testControlScheme();
  testPlayerMotionPlan();
  testSwimMotionPlan();
  testClimbMotionPlan();
  testAmphibiousPredatorMotion();
  testCaveSkitterGroupPatrolsAndBitesInsideWeb();
  testAvatarRigSceneBinding();
  testThirdPersonFollowController();
  std::cout << "systems_tests passed.\n";
  return 0;
}
