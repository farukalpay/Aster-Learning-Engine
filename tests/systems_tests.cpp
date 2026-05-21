// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "test_support.hpp"

namespace {

aster::FeatureLabel featureLabel(const std::string_view text) {
  const std::optional<aster::FeatureLabel> label = aster::parseFeatureLabel(text);
  assert(label.has_value());
  return *label;
}

aster::ActionLaneSet actionLane(std::string_view lane) {
  aster::ActionLaneSet lanes;
  assert(lanes.add(lane));
  return lanes;
}

aster::FeatureLabelSet labelSet(std::initializer_list<std::string_view> labels) {
  aster::FeatureLabelSet result;
  for (const std::string_view label : labels) {
    assert(result.add(label));
  }
  return result;
}

void testFeatureLabelContracts() {
  const aster::FeatureLabel climb = featureLabel(" Player/Motion:Climb ");
  assert(climb.path() == "player.motion.climb");
  assert(climb.depth() == 3u);
  assert(climb.parent().has_value());
  assert(climb.parent()->path() == "player.motion");
  assert(climb.matches(featureLabel("player.motion")));
  assert(!featureLabel("player.motion").matches(climb));

  const std::vector<aster::FeatureLabel> lineage = climb.lineage();
  assert(lineage.size() == 3u);
  assert(lineage.front().path() == "player");
  assert(lineage.back() == climb);

  aster::FeatureLabelSet actor_labels;
  assert(actor_labels.add(climb));
  assert(actor_labels.add("surface.wet.rock"));
  assert(actor_labels.add("interaction.pickup"));
  assert(actor_labels.contains("player.motion"));
  assert(!actor_labels.contains("player.motion", aster::FeatureLabelMatchMode::Exact));
  assert(actor_labels.contains("player.motion.climb", aster::FeatureLabelMatchMode::Exact));
  assert(!actor_labels.add("player.motion.climb"));

  const aster::FeatureLabelQuery climb_or_wet{
      .all = {featureLabel("player.motion")},
      .any = {featureLabel("surface.wet"), featureLabel("surface.ice")},
      .none = {featureLabel("state.stunned")}};
  assert(climb_or_wet.matches(actor_labels));

  const aster::FeatureLabelQuery blocked_by_wet{
      .all = {featureLabel("player.motion")},
      .none = {featureLabel("surface.wet")}};
  assert(!blocked_by_wet.matches(actor_labels));
  assert(!blocked_by_wet.explainMismatch(actor_labels).empty());

  aster::FeatureLabelSet same_labels_different_order;
  assert(same_labels_different_order.add("interaction.pickup"));
  assert(same_labels_different_order.add("surface.wet.rock"));
  assert(same_labels_different_order.add("player.motion.climb"));
  assert(same_labels_different_order.contractStamp() == actor_labels.contractStamp());

  aster::FeatureLabelSet swim_labels;
  assert(swim_labels.add("player.motion.swim"));
  const float affinity = actor_labels.lineageAffinity(swim_labels);
  assert(affinity > 0.20f && affinity < 1.0f);

  aster::FeatureLabelCatalog catalog;
  assert(catalog.declare("player.motion.climb", "Climb Locomotion",
                         "Author-owned locomotion capability imported into Aster."));
  assert(catalog.addAlias("climb", "player.motion.climb"));
  const std::optional<aster::FeatureLabel> resolved = catalog.resolve("climb");
  assert(resolved.has_value());
  assert(*resolved == climb);
  assert(catalog.find(climb)->display_name == "Climb Locomotion");
}

void testActionSchedulerFeatureContracts() {
  aster::FeatureLabelSet world = labelSet({"world.cave.lit", "actor.player.grounded"});
  aster::ActionScheduler scheduler;
  scheduler.setWorldLabels(world);

  int walk_ticks = 0;
  bool walk_cancelled = false;
  const aster::ActionTaskId walk = scheduler.submit({
      .name = "lumen walk tunnel",
      .owner = "lumen",
      .priority = aster::ActionTaskPriority::Normal,
      .lanes = actionLane("lane.actor.motion"),
      .labels = labelSet({"player.motion.walk"}),
      .duration_seconds = 1.0f,
      .on_tick = [&](aster::ActionTaskContext &) { ++walk_ticks; },
      .on_finish = [&](aster::ActionTaskContext &) { walk_cancelled = true; },
  });

  aster::ActionSchedulerFrame frame = scheduler.tick(0.10f);
  assert(frame.active_tasks == 1u);
  assert(walk_ticks == 1);
  assert(scheduler.find(walk)->state == aster::ActionTaskState::Active);
  assert(scheduler.activeLanes().contains(featureLabel("lane.actor.motion")));

  bool dodge_started = false;
  const aster::ActionTaskId dodge = scheduler.submit({
      .name = "lumen ledge dodge",
      .owner = "lumen",
      .priority = aster::ActionTaskPriority::Critical,
      .lanes = actionLane("lane.actor.motion"),
      .labels = labelSet({"player.motion.dodge", "surface.ledge"}),
      .requirements = {.all = {featureLabel("actor.player.grounded")}},
      .duration_seconds = 0.10f,
      .can_preempt = true,
      .on_start = [&](aster::ActionTaskContext &) { dodge_started = true; },
  });

  frame = scheduler.tick(0.10f);
  assert(walk_cancelled);
  assert(dodge_started);
  assert(scheduler.find(walk)->state == aster::ActionTaskState::Cancelled);
  assert(scheduler.find(dodge)->state == aster::ActionTaskState::Finished);
  bool saw_preemption = false;
  for (const aster::ActionTaskEvent &event : frame.events) {
    saw_preemption = saw_preemption || event.kind == aster::ActionTaskEventKind::Cancelled;
  }
  assert(saw_preemption);

  const aster::ActionTaskId swim = scheduler.submit({
      .name = "deep water swim gate",
      .owner = "lumen",
      .priority = aster::ActionTaskPriority::High,
      .lanes = actionLane("lane.actor.motion"),
      .labels = labelSet({"player.motion.swim"}),
      .requirements = {.all = {featureLabel("world.water.deep")}},
      .duration_seconds = 0.05f,
  });
  scheduler.tick(0.0f);
  assert(scheduler.find(swim)->state == aster::ActionTaskState::Blocked);

  assert(world.add("world.water.deep"));
  scheduler.setWorldLabels(world);
  scheduler.tick(0.05f);
  assert(scheduler.find(swim)->state == aster::ActionTaskState::Finished);
  assert(scheduler.events().size() >= 8u);

  aster::ActionScheduler resource_scheduler;
  aster::ActionResourceSet motion_resource;
  assert(motion_resource.add("resource.actor.motion"));
  resource_scheduler.beginEventBatch();
  const aster::ActionTaskId long_task = resource_scheduler.submit({
      .name = "resource holder",
      .owner = "actor.one",
      .priority = aster::ActionTaskPriority::Normal,
      .claimed_resources = motion_resource,
      .duration_seconds = 1.0f,
  });
  resource_scheduler.tick(0.10f);
  assert(resource_scheduler.events().empty());
  resource_scheduler.endEventBatch();
  assert(!resource_scheduler.events().empty());
  assert(resource_scheduler.find(long_task)->state == aster::ActionTaskState::Active);
  assert(resource_scheduler.claimedResources().contains(featureLabel("resource.actor.motion")));

  const aster::ActionTaskId blocked = resource_scheduler.submit({
      .name = "resource waiter",
      .owner = "actor.two",
      .priority = aster::ActionTaskPriority::Normal,
      .required_resources = motion_resource,
      .duration_seconds = 0.1f,
  });
  resource_scheduler.tick(0.01f);
  assert(resource_scheduler.find(blocked)->state == aster::ActionTaskState::Blocked);
  assert(resource_scheduler.find(blocked)->blocked_by_tasks.size() == 1u);
  assert(resource_scheduler.find(blocked)->blocked_by_tasks.front() == long_task);
  assert(!resource_scheduler.find(blocked)->owner_diagnostic.empty());

  bool preempt_finish_called = false;
  const aster::ActionTaskId preempt = resource_scheduler.submit({
      .name = "resource preempt",
      .owner = "actor.two",
      .priority = aster::ActionTaskPriority::Critical,
      .claimed_resources = motion_resource,
      .duration_seconds = 0.01f,
      .can_preempt = true,
      .on_finish = [&](aster::ActionTaskContext &context) {
        assert(context.claimed_resources.contains(featureLabel("resource.actor.motion")));
        preempt_finish_called = true;
      },
  });
  resource_scheduler.tick(0.02f);
  assert(resource_scheduler.find(long_task)->state == aster::ActionTaskState::Cancelled);
  assert(resource_scheduler.find(preempt)->state == aster::ActionTaskState::Finished);
  assert(preempt_finish_called);
  assert(!resource_scheduler.journal().empty());
  assert(resource_scheduler.journal().contractStamp() != 0u);
  assert(resource_scheduler.journal().summary().find("actor.two") != std::string::npos);
  const std::vector<aster::ActionOwnerDiagnostic> owner_diagnostics =
      resource_scheduler.ownerDiagnostics();
  assert(!owner_diagnostics.empty());
}

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

void testDeterministicCommandReplayAndLegacyArchive() {
  aster::DeterministicRandomStream a(42u);
  aster::DeterministicRandomStream b(42u);
  assert(a.nextU32() == b.nextU32());
  assert(a.nextByte() == b.nextByte());

  aster::SimCommand command;
  command.tick = 7u;
  command.forward = 1200;
  command.strafe = -300;
  command.set(aster::SimCommandButton::Interact, true);
  aster::CommandReplay replay;
  replay.record(command);
  command.tick = 8u;
  command.sequence = 2u;
  replay.record(command);
  assert(replay.find(7u) != nullptr);
  assert(replay.checksum() == replay.checksum());

  const std::filesystem::path wad_path =
      std::filesystem::temp_directory_path() / "aster_legacy_lump_archive_test.wad";
  {
    std::ofstream wad(wad_path, std::ios::binary);
    const auto write_u32 = [&wad](const std::uint32_t value) {
      const char bytes[4] = {static_cast<char>(value & 0xffu),
                             static_cast<char>((value >> 8u) & 0xffu),
                             static_cast<char>((value >> 16u) & 0xffu),
                             static_cast<char>((value >> 24u) & 0xffu)};
      wad.write(bytes, 4);
    };
    wad.write("PWAD", 4);
    write_u32(1u);
    write_u32(16u);
    wad.write("ABCD", 4);
    write_u32(12u);
    write_u32(4u);
    wad.write("MAPA", 4);
    wad.put('\0');
    wad.put('\0');
    wad.put('\0');
    wad.put('\0');
  }
  aster::LegacyLumpArchive archive;
  const bool added = archive.addFile(wad_path, true);
  if (!added) {
    throw std::runtime_error("Failed to load temporary legacy lump archive.");
  }
  const std::size_t index = archive.require("mapa");
  assert(archive.record(index)->name == "MAPA");
  const std::vector<std::uint8_t> bytes = archive.read(index);
  assert(bytes.size() == 4u);
  assert(bytes[0] == static_cast<std::uint8_t>('A'));
  assert(archive.cache(index).size() == 4u);
  assert(archive.reload());
  assert(archive.profile().front().cached);
  std::filesystem::remove(wad_path);
}

void testClassicActorMechanismAutomapAndWipe() {
  aster::ClassicActorRuntime actors;
  actors.spawn({.id = "actor.one",
                .position = {0.0f, 0.0f, 0.0f},
                .home = {0.0f, 0.0f, 0.0f},
                .speed = 1.0f,
                .notice_radius = 5.0f,
                .strike_radius = 0.45f,
                .strike_cooldown = 0.1f,
                .health = 2});
  aster::ClassicActorFrame frame = actors.update({0.0f, 0.0f, 3.0f}, 1.0f);
  assert(!frame.events.empty());
  assert(actors.find("actor.one")->mode == aster::ClassicActorMode::Chase ||
         actors.find("actor.one")->mode == aster::ClassicActorMode::Alert);
  assert(actors.damage("actor.one", 2));
  actors.update({0.0f, 0.0f, 0.1f}, 0.1f);
  assert(actors.find("actor.one")->mode == aster::ClassicActorMode::Dead);

  aster::WorldMechanismSystem mechanisms;
  mechanisms.add({.id = "door",
                  .kind = aster::WorldMechanismKind::Door,
                  .closed_position = {0.0f, 0.0f, 0.0f},
                  .open_position = {0.0f, 1.0f, 0.0f},
                  .speed = 2.0f});
  assert(mechanisms.trigger("door"));
  mechanisms.update(0.25f);
  const aster::WorldMechanismState *door = mechanisms.find("door");
  assert(door != nullptr);
  assert(door->progress > 0.0f);
  assert(door->position.y > 0.0f);

  aster::AutomapModel map;
  map.addLine({{0.0f, 0.0f}, {4.0f, 0.0f}});
  map.addMarker({"door", aster::AutomapMarkerKind::Door, {2.0f, 0.0f}});
  map.setPlayer({1.0f, 0.0f}, 0.0f);
  map.revealWithin({1.0f, 0.0f}, 2.0f);
  assert(map.hasDiscovery());
  assert(map.project({1.0f, 0.0f}, {200.0f, 120.0f}).visible);

  aster::TransitionWipe wipe;
  wipe.start(8, 100, 3u, 0.5f);
  wipe.update(0.25f);
  const aster::TransitionWipeFrame wipe_frame = wipe.frame();
  assert(wipe_frame.active);
  assert(wipe_frame.column_progress.size() == 8u);

  const aster::ClassicHudSignalModel signals = aster::evaluateClassicHudSignals(
      {.health = 4, .max_health = 10, .gauntlet_active = true, .threat_visible = true});
  assert(signals.visible);
  assert(signals.health_fraction < 0.5f);
  assert(signals.threat > 0.0f);
}

} // namespace

int main() {
  testFeatureLabelContracts();
  testActionSchedulerFeatureContracts();
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
  testDeterministicCommandReplayAndLegacyArchive();
  testClassicActorMechanismAutomapAndWipe();
  std::cout << "systems_tests passed.\n";
  return 0;
}
