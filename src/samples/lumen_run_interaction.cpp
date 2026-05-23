// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "lumen_run_detail.hpp"

namespace aster {

// Chest, equipment, relay, and cave interaction visuals.
void LumenRun::updateChestInteractionState() {
  ASTER_PROFILE_SCOPE("LumenRun::updateChestInteractionState");
  if (!chest_open_) {
    return;
  }
  const Vec3 chest_focus = chest_base_ + Vec3{0.0f, 0.46f, 0.0f};
  const float distance = length(player_position_ - chest_focus);
  if (!chest_distance_close_armed_ && distance <= kChestInteractionDistance) {
    chest_distance_close_armed_ = true;
  }
  if (chest_distance_close_armed_ && distance > kChestAutoCloseDistance) {
    closeChest();
  }
}

void LumenRun::updateChestVisuals(const float dt) {
  chest_lid_animation_.update(dt);
  auto &objects = scene_.objects();
  const auto hideObject = [&](const std::size_t index) {
    if (index >= objects.size()) {
      return;
    }
    objects[index].transform.position = {0.0f, -20.0f, 0.0f};
    objects[index].transform.scale = {0.001f, 0.001f, 0.001f};
    objects[index].material.emission_strength = 0.0f;
  };
  const auto placeLidPart = [&](const std::size_t index, const Vec3 closed_local) {
    if (index >= objects.size()) {
      return;
    }
    const float open = easeOutCubic(chest_lid_animation_.value());
    const float angle = open * radians(58.0f);
    const Vec3 hinge{0.0f, 0.39f, -0.25f};
    const Vec3 opened_local = hinge + rotateX(closed_local - hinge, -angle);
    RenderObject &object = objects[index];
    object.transform.position = chest_base_ + rotateYaw(opened_local, chest_yaw_);
    object.transform.rotation = quatFromEulerXyz({-angle, chest_yaw_, 0.0f});
  };

  placeLidPart(chest_lid_object_, {0.0f, 0.49f, 0.0f});
  placeLidPart(chest_lid_band_object_, {0.0f, 0.44f, 0.326f});
  placeLidPart(chest_lock_object_, {0.0f, 0.36f, 0.354f});

  const float open_visibility = easeOutCubic(chest_lid_animation_.value());
  const InteractionFocus &focus = interaction_.focus();
  for (ChestItemVisual &item : chest_items_) {
    const bool visible = item.available && open_visibility > 0.01f;
    const bool highlighted = focus.visible && focus.target_id == "item:" + item.item_id;
    for (const ChestItemPart &part : item.parts) {
      if (!visible) {
        hideObject(part.object_index);
        continue;
      }
      if (part.object_index >= objects.size()) {
        continue;
      }
      RenderObject &object = objects[part.object_index];
      const float lift =
          std::sin(status_.elapsed_seconds * 3.2f + static_cast<float>(part.object_index) * 0.17f) *
          0.010f * open_visibility;
      object.transform.position =
          chest_base_ + rotateYaw(part.local_position + Vec3{0.0f, lift, 0.0f}, chest_yaw_);
      object.transform.rotation = quatFromEulerXyz(
          {part.local_rotation.x, chest_yaw_ + part.local_rotation.y, part.local_rotation.z});
      object.transform.scale = part.scale * open_visibility;
      object.material.emission_strength = part.base_emission + (highlighted ? 0.16f : 0.0f);
    }
  }
}

void LumenRun::updateEquipmentVisuals(const float dt) {
  auto &objects = scene_.objects();
  const auto hideObject = [&](const std::size_t index) {
    if (index >= objects.size()) {
      return;
    }
    objects[index].transform.position = {0.0f, -20.0f, 0.0f};
    objects[index].transform.scale = {0.001f, 0.001f, 0.001f};
  };

  const std::string equipped_id =
      equipment_.hasEquippedItem() ? equipment_.equipped().item_id : std::string{};
  Transform hand_socket =
      Transform::fromEuler(avatarPosePosition() +
                               rotateYaw({0.27f, 0.33f, 0.11f}, player_facing_yaw_),
                           {0.0f, player_facing_yaw_, 0.0f});
  if (const std::optional<Transform> resolved_socket = resolveAvatarAttachmentSocket(
          player_avatar_, player_avatar_pose_, plushRightHandCarrySocket())) {
    hand_socket = *resolved_socket;
  }
  const float stride = std::clamp(player_avatar_pose_.stride_amplitude, 0.0f, 1.0f);
  const float carry_swing = std::sin(player_avatar_pose_.gait_phase) * stride;
  const Vec3 carry_rotation{radians(-12.0f) + carry_swing * 0.08f,
                            player_facing_yaw_ + radians(5.0f),
                            radians(-10.0f) + carry_swing * 0.06f};
  for (const EquippedItemPart &part : equipped_item_parts_) {
    if (part.item_id != equipped_id) {
      hideObject(part.object_index);
      continue;
    }
    if (part.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[part.object_index];
    object.transform.position =
        hand_socket.position + rotateEuler(part.local_position, carry_rotation);
    object.transform.rotation = quatFromEulerXyz(carry_rotation + part.local_rotation);
    object.transform.scale = part.scale;
  }

  const bool torch_equipped = equipment_.isEquipped("torch");
  const Vec3 flame_position =
      hand_socket.position + rotateEuler({0.0f, 0.39f, 0.0f}, carry_rotation);
  equipped_light_ = torch_equipped ? evaluateFlickerLight({.color = {1.0f, 0.42f, 0.16f},
                                                           .intensity = 7.4f,
                                                           .amplitude = 0.22f,
                                                           .speed = 14.0f,
                                                           .source_radius = 1.08f},
                                                          flame_position, status_.elapsed_seconds)
                                   : DynamicPointLight{};

  if (!torch_equipped) {
    torch_flame_.reset();
    for (const TorchParticleVisual &visual : torch_particle_visuals_) {
      hideObject(visual.object_index);
    }
    return;
  }

  torch_flame_.update(dt, flame_position, status_.elapsed_seconds,
                      {.max_particles = torch_particle_visuals_.size(),
                       .lifetime = 0.48f,
                       .rise_speed = 0.48f,
                       .swirl_radius = 0.045f,
                       .base_size = 0.040f,
                       .hot_tint = {1.0f, 0.46f, 0.08f},
                       .cool_tint = {0.55f, 0.07f, 0.018f}});
  const std::vector<Particle> &particles = torch_flame_.particles();
  for (std::size_t i = 0; i < torch_particle_visuals_.size(); ++i) {
    const std::size_t object_index = torch_particle_visuals_[i].object_index;
    if (object_index >= objects.size()) {
      continue;
    }
    if (i >= particles.size() || !particles[i].active) {
      hideObject(object_index);
      continue;
    }
    const Particle &particle = particles[i];
    RenderObject &object = objects[object_index];
    object.transform.position = particle.position;
    object.transform.rotation =
        quatFromEulerXyz({0.0f, status_.elapsed_seconds * 1.6f + static_cast<float>(i), 0.0f});
    object.transform.scale = {particle.size, particle.size * 1.55f, particle.size};
    object.material.base_color = LinearRgb{particle.tint};
    object.material.emission_color = EmissionColor{particle.tint};
    object.material.emission_strength = 0.66f;
  }
}

void LumenRun::activatePrismRelay() {
  prism_relay_active_ = true;
  prism_relay_charge_ =
      std::max(prism_relay_charge_, std::min(kPrismRelayIgnitedCharge + kPrismRelayRetuneKick,
                                             kPrismRelayIgnitedCharge * 1.15f));
}

void LumenRun::updatePrismRelay(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updatePrismRelay");
  const float target = prism_relay_active_ ? kPrismRelayIgnitedCharge : kPrismRelayIdleCharge;
  const float response = prism_relay_active_ ? 2.7f : 1.4f;
  const float blend = clamp(dt * response, 0.0f, 1.0f);
  prism_relay_charge_ = std::lerp(prism_relay_charge_, target, blend);
}

void LumenRun::updatePrismRelayVisuals(const float dt) {
  (void)dt;
  auto &objects = scene_.objects();
  const float charge = clamp(prism_relay_charge_, 0.0f, 1.15f);
  const float field = clamp((charge - 0.05f) / 0.95f, 0.0f, 1.0f);
  const float pulse = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 3.35f);
  const Vec3 relay_core = prismRelayFocusPosition();

  if (prism_relay_core_valid_ && prism_relay_core_object_ < objects.size()) {
    RenderObject &core = objects[prism_relay_core_object_];
    core.transform.position = relay_core + Vec3{0.0f, 0.035f * field * pulse, 0.0f};
    core.transform.rotation =
        quatFromEulerXyz({0.045f * std::sin(status_.elapsed_seconds * 1.7f),
                          status_.elapsed_seconds * (0.32f + field * 0.22f),
                          0.050f * std::cos(status_.elapsed_seconds * 1.3f)});
    const float breathe = 1.0f + field * (0.026f + 0.018f * pulse);
    core.transform.scale = {0.24f * breathe, 0.52f * breathe, 0.24f * breathe};
    core.material.emission_strength = 0.10f + charge * (0.56f + 0.20f * pulse);
  }

  for (std::size_t i = 0; i < prism_relay_ring_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_ring_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &ring = objects[object_index];
    const float direction = i == 0u ? 1.0f : -1.0f;
    ring.transform.position = relay_core;
    ring.transform.rotation =
        quatFromEulerXyz(i == 0u
                             ? Vec3{0.08f * field * std::sin(status_.elapsed_seconds * 1.1f),
                                    status_.elapsed_seconds * 0.24f * direction,
                                    0.05f * field * std::cos(status_.elapsed_seconds * 1.6f)}
                             : Vec3{radians(90.0f) +
                                        0.07f * field *
                                            std::sin(status_.elapsed_seconds * 1.4f),
                                    status_.elapsed_seconds * 0.18f * direction,
                                    radians(12.0f) +
                                        0.04f * field *
                                            std::cos(status_.elapsed_seconds * 1.9f)});
    const float ring_breathe = 1.0f + field * (0.035f + 0.018f * pulse);
    ring.transform.scale = {ring_breathe, ring_breathe, ring_breathe};
    ring.material.opacity = 0.08f + field * 0.34f;
    ring.material.emission_strength = 0.04f + charge * (0.42f + 0.16f * pulse);
  }

  for (std::size_t i = 0; i < prism_relay_conduit_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_conduit_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &conduit = objects[object_index];
    const float phase = pulse + 0.5f * std::sin(status_.elapsed_seconds * 2.2f +
                                                static_cast<float>(i) * 1.37f);
    conduit.material.opacity = 0.045f + field * (0.22f + 0.08f * phase);
    conduit.material.emission_strength = 0.035f + charge * (0.46f + 0.18f * phase);
  }

  for (std::size_t i = 0; i < prism_relay_node_objects_.size(); ++i) {
    const std::size_t object_index = prism_relay_node_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    RenderObject &node = objects[object_index];
    const float phase = 0.5f + 0.5f * std::sin(status_.elapsed_seconds * 2.6f +
                                               static_cast<float>(i) * 0.91f);
    const float scale = 0.11f + field * (0.035f + 0.018f * phase);
    node.transform.scale = {scale, scale, scale};
    node.material.emission_strength = 0.08f + charge * (0.38f + 0.12f * phase);
  }
}

void LumenRun::refreshClassicGauntletAutomap() {
  classic_gauntlet_automap_.clear();
  const Vec2 entry{classic_gauntlet_entry_.x, classic_gauntlet_entry_.z};
  const Vec2 plate{classic_gauntlet_plate_.x, classic_gauntlet_plate_.z};
  const Vec2 door{classic_gauntlet_door_center_.x, classic_gauntlet_door_center_.z};
  const Vec2 lift{classic_gauntlet_lift_base_.x, classic_gauntlet_lift_base_.z};
  const Vec2 exit{classic_gauntlet_exit_.x, classic_gauntlet_exit_.z};
  classic_gauntlet_automap_.addLine({entry, plate, false, false});
  classic_gauntlet_automap_.addLine({plate, door, false, true});
  classic_gauntlet_automap_.addLine({door, lift, false, false});
  classic_gauntlet_automap_.addLine({lift, exit, false, false});
  classic_gauntlet_automap_.addMarker({"bulkhead", AutomapMarkerKind::Door, door, false, 0.0f});
  classic_gauntlet_automap_.addMarker({"lift", AutomapMarkerKind::Lift, lift, false, 0.0f});
  classic_gauntlet_automap_.addMarker({"exit", AutomapMarkerKind::Objective, exit, false, 0.0f});
  for (const ClassicGauntletActorVisual &visual : classic_gauntlet_actor_visuals_) {
    if (const ClassicActorState *actor = classic_gauntlet_actors_.find(visual.id)) {
      classic_gauntlet_automap_.addMarker(
          {visual.id, AutomapMarkerKind::Threat, {actor->position.x, actor->position.z}, false,
           0.0f});
    }
  }
}

void LumenRun::updateClassicGauntlet(const float dt) {
  ASTER_PROFILE_SCOPE("LumenRun::updateClassicGauntlet");
  if (classic_gauntlet_actor_visuals_.empty()) {
    classic_gauntlet_hud_ = {};
    return;
  }

  classic_gauntlet_hurt_seconds_ = std::max(0.0f, classic_gauntlet_hurt_seconds_ - dt);
  const float entry_distance = distanceOnArena(player_position_, classic_gauntlet_entry_);
  const float plate_distance = distanceOnArena(player_position_, classic_gauntlet_plate_);
  const bool entered = entry_distance < 4.2f || plate_distance < 3.0f;
  if (entered) {
    classic_gauntlet_active_ = true;
    classic_gauntlet_discovered_ = true;
    if (!classic_gauntlet_wipe_started_) {
      classic_gauntlet_wipe_.start(72, 720, 0xA57E901Du, 0.72f);
      classic_gauntlet_wipe_started_ = true;
    }
  }

  if (classic_gauntlet_active_) {
    classic_gauntlet_mechanisms_.trigger("classic.bulkhead", true);
    if (plate_distance < 1.9f) {
      classic_gauntlet_mechanisms_.trigger("classic.lift", true);
    }
  }

  classic_gauntlet_mechanisms_.update(dt);
  if (const WorldMechanismState *bulkhead =
          classic_gauntlet_mechanisms_.find("classic.bulkhead")) {
    if (bulkhead->progress > 0.74f &&
        distanceOnArena(player_position_, classic_gauntlet_lift_base_) < 2.6f) {
      classic_gauntlet_mechanisms_.trigger("classic.lift", true);
    }
  }

  if (classic_gauntlet_active_) {
    const int health_before = status_.health;
    const ClassicActorFrame frame = classic_gauntlet_actors_.update(player_position_, dt);
    for (const ClassicActorEvent &event : frame.events) {
      if (event.strike) {
        const ClassicActorState *actor = classic_gauntlet_actors_.find(event.actor_id);
        (void)applyPlayerDamage(2, actor != nullptr ? actor->position : classic_gauntlet_entry_);
      }
    }
    if (status_.health < health_before) {
      classic_gauntlet_hurt_seconds_ = 0.54f;
    }
  }

  classic_gauntlet_wipe_.update(dt);
  classic_gauntlet_automap_.setPlayer({player_position_.x, player_position_.z},
                                      player_facing_yaw_);
  if (classic_gauntlet_discovered_) {
    classic_gauntlet_automap_.revealWithin({player_position_.x, player_position_.z}, 7.4f);
  }

  bool threat_visible = false;
  for (const ClassicActorState &actor : classic_gauntlet_actors_.actors()) {
    threat_visible = threat_visible ||
                     (actor.mode == ClassicActorMode::Chase ||
                      actor.mode == ClassicActorMode::Strike ||
                      actor.mode == ClassicActorMode::Alert);
  }
  bool mechanism_active = false;
  for (const WorldMechanismState &state : classic_gauntlet_mechanisms_.states()) {
    mechanism_active = mechanism_active || state.mode == WorldMechanismMode::Opening ||
                       state.mode == WorldMechanismMode::Open;
  }
  classic_gauntlet_hud_ = evaluateClassicHudSignals({.health = status_.health,
                                                     .max_health = status_.max_health,
                                                     .gauntlet_active = classic_gauntlet_active_,
                                                     .threat_visible = threat_visible,
                                                     .mechanism_active = mechanism_active,
                                                     .hurt_seconds = classic_gauntlet_hurt_seconds_});
}

void LumenRun::updateClassicGauntletVisuals(const float dt) {
  (void)dt;
  auto &objects = scene_.objects();
  const WorldMechanismState *bulkhead = classic_gauntlet_mechanisms_.find("classic.bulkhead");
  const float door_open = bulkhead != nullptr ? clamp(bulkhead->progress, 0.0f, 1.0f) : 0.0f;
  for (std::size_t i = 0; i < classic_gauntlet_door_objects_.size(); ++i) {
    const std::size_t object_index = classic_gauntlet_door_objects_[i];
    if (object_index >= objects.size()) {
      continue;
    }
    const float side = i == 0u ? -1.0f : 1.0f;
    RenderObject &door = objects[object_index];
    door.transform.position =
        classic_gauntlet_door_center_ + classic_gauntlet_door_side_ * (side * (0.42f + door_open * 0.66f));
    door.material.emission_strength = 0.04f + door_open * 0.10f;
  }

  if (const WorldMechanismState *lift = classic_gauntlet_mechanisms_.find("classic.lift")) {
    if (classic_gauntlet_lift_object_ < objects.size()) {
      RenderObject &platform = objects[classic_gauntlet_lift_object_];
      platform.transform.position = lift->position;
      platform.material.emission_strength = 0.06f + lift->progress * 0.12f;
    }
  }

  for (std::size_t i = 0; i < classic_gauntlet_signal_objects_.size(); ++i) {
    if (classic_gauntlet_signal_objects_[i] >= objects.size()) {
      continue;
    }
    RenderObject &signal = objects[classic_gauntlet_signal_objects_[i]];
    const float pulse =
        0.5f + 0.5f * std::sin(status_.elapsed_seconds * 5.2f + static_cast<float>(i) * 0.71f);
    signal.material.emission_strength =
        classic_gauntlet_active_ ? 0.18f + pulse * 0.18f : 0.035f + pulse * 0.025f;
  }

  for (const ClassicGauntletActorVisual &visual : classic_gauntlet_actor_visuals_) {
    if (visual.object_index >= objects.size()) {
      continue;
    }
    const ClassicActorState *actor = classic_gauntlet_actors_.find(visual.id);
    if (actor == nullptr || actor->mode == ClassicActorMode::Dead) {
      hideRenderObject(objects[visual.object_index]);
      if (visual.eye_object_index < objects.size()) {
        hideRenderObject(objects[visual.eye_object_index]);
      }
      if (visual.beacon_object_index < objects.size()) {
        hideRenderObject(objects[visual.beacon_object_index]);
      }
      continue;
    }
    RenderObject &object = objects[visual.object_index];
    const float alert = actor->mode == ClassicActorMode::Chase ||
                                actor->mode == ClassicActorMode::Strike
                            ? 1.0f
                            : 0.0f;
    const Vec3 forward{std::sin(actor->facing_yaw), 0.0f, std::cos(actor->facing_yaw)};
    object.transform.position = actor->position + Vec3{0.0f, 0.14f, 0.0f};
    object.transform.rotation =
        quatFromEulerXyz({0.0f, actor->facing_yaw, 0.055f * std::sin(actor->age * 7.0f)});
    object.transform.scale = {1.62f + alert * 0.16f, 1.50f + alert * 0.08f,
                              1.62f + alert * 0.16f};
    object.material.emission_strength = 0.24f + alert * 0.32f;
    if (visual.eye_object_index < objects.size()) {
      RenderObject &eye = objects[visual.eye_object_index];
      eye.transform.position = actor->position + forward * 0.40f + Vec3{0.0f, 0.34f, 0.0f};
      eye.transform.rotation = quatFromEulerXyz({0.0f, actor->facing_yaw, 0.0f});
      const float eye_pulse = 0.5f + 0.5f * std::sin(actor->age * (alert > 0.0f ? 15.0f : 8.0f));
      eye.transform.scale = {0.105f + eye_pulse * 0.018f, 0.070f, 0.105f + eye_pulse * 0.018f};
      eye.material.emission_strength = 0.40f + alert * 0.40f + eye_pulse * 0.16f;
    }
    if (visual.beacon_object_index < objects.size()) {
      RenderObject &beacon = objects[visual.beacon_object_index];
      beacon.transform.position = actor->position + Vec3{0.0f, 0.60f + alert * 0.06f, 0.0f};
      beacon.transform.rotation = quatFromEulerXyz({0.0f, actor->facing_yaw + actor->age * 2.4f, 0.0f});
      const float beacon_pulse = 0.5f + 0.5f * std::sin(actor->age * 10.0f);
      beacon.transform.scale = {0.080f + alert * 0.040f, 0.13f + beacon_pulse * 0.040f,
                                0.080f + alert * 0.040f};
      beacon.material.emission_strength = 0.28f + alert * 0.50f + beacon_pulse * 0.14f;
    }
  }
}

void LumenRun::updateCaveVisuals(const float dt) {
  auto &objects = scene_.objects();
  CaveInteriorSample player_cave_sample{};
  (void)caveSectionAt(player_position_, &player_cave_sample);
  const PerceptualWorldScheduleReport &schedule = world_forensics_.perceptual_schedule;
  const float exterior_opacity = player_cave_sample.interior > 0.08f ? 0.0f : 1.0f;
  const Vec3 exterior_scale = exterior_opacity <= 0.0f ? Vec3{0.001f, 0.001f, 0.001f}
                                                       : Vec3{1.0f, 1.0f, 1.0f};
  for (const std::size_t object_index : cave_exterior_hidden_objects_) {
    if (object_index < objects.size()) {
      RenderObject &object = objects[object_index];
      object.transform.scale = exterior_scale;
      object.material.opacity = exterior_opacity;
    }
  }
  for (CoalOreNode &ore : coal_ores_) {
    if (ore.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[ore.object_index];
    if (ore.collected) {
      object.transform.position = {0.0f, -20.0f, 0.0f};
      object.transform.scale = {0.001f, 0.001f, 0.001f};
      object.material.emission_strength = 0.0f;
      continue;
    }
    ore.hit_flash = std::max(0.0f, ore.hit_flash - dt * 3.8f);
    const float damage = 1.0f - static_cast<float>(std::max(ore.health, 0)) /
                                    static_cast<float>(std::max(ore.max_health, 1));
    const float pulse = ore.hit_flash * 0.06f + damage * 0.035f;
    object.transform.position = ore.position + ore.normal * (ore.hit_flash * 0.018f);
    object.transform.scale = ore.scale * (1.0f + pulse);
    object.material.emission_strength = 0.115f + ore.hit_flash * 0.120f + damage * 0.030f;
    object.material.edge_wear = 0.18f + damage * 0.38f + schedule.material_age * 0.10f;
    object.material.procedural.wetness =
        std::max(object.material.procedural.wetness, schedule.material_age * 0.08f);
    object.material.pattern_contrast =
        std::max(object.material.pattern_contrast, 0.18f + schedule.memory_residue * 0.18f);
  }
  for (CaveWebObstacle &web : cave_webs_) {
    if (web.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[web.object_index];
    if (web.broken) {
      hideRenderObject(object);
      continue;
    }
    web.hit_flash = std::max(0.0f, web.hit_flash - dt * 4.4f);
    object.material.emission_strength = 0.035f + web.hit_flash * 0.18f;
    object.material.opacity = 0.74f + web.hit_flash * 0.10f;
    object.material.edge_wear = std::max(object.material.edge_wear, schedule.interaction_debt * 0.16f);
  }
}

} // namespace aster
