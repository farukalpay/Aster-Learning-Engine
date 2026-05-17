// Author: Faruk Alpay
// Do not remove this notice.

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
    object.transform.rotation = {-angle, chest_yaw_, 0.0f};
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
      object.transform.rotation = {part.local_rotation.x, chest_yaw_ + part.local_rotation.y,
                                   part.local_rotation.z};
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
  Transform hand_socket{avatarPosePosition() + rotateYaw({0.27f, 0.33f, 0.11f}, player_facing_yaw_),
                        {0.0f, player_facing_yaw_, 0.0f},
                        {1.0f, 1.0f, 1.0f}};
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
    object.transform.rotation = carry_rotation + part.local_rotation;
    object.transform.scale = part.scale;
  }

  const bool torch_equipped = equipment_.isEquipped("torch");
  const Vec3 flame_position =
      hand_socket.position + rotateEuler({0.0f, 0.39f, 0.0f}, carry_rotation);
  equipped_light_ = torch_equipped ? evaluateFlickerLight({.color = {1.0f, 0.50f, 0.20f},
                                                           .intensity = 5.8f,
                                                           .amplitude = 0.22f,
                                                           .speed = 14.0f,
                                                           .source_radius = 0.82f},
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
    object.transform.rotation = {0.0f, status_.elapsed_seconds * 1.6f + static_cast<float>(i),
                                 0.0f};
    object.transform.scale = {particle.size, particle.size * 1.55f, particle.size};
    object.material.base_color = particle.tint;
    object.material.emission_color = particle.tint;
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
    core.transform.rotation = {0.045f * std::sin(status_.elapsed_seconds * 1.7f),
                               status_.elapsed_seconds * (0.32f + field * 0.22f),
                               0.050f * std::cos(status_.elapsed_seconds * 1.3f)};
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
        i == 0u
            ? Vec3{0.08f * field * std::sin(status_.elapsed_seconds * 1.1f),
                   status_.elapsed_seconds * 0.24f * direction,
                   0.05f * field * std::cos(status_.elapsed_seconds * 1.6f)}
            : Vec3{radians(90.0f) + 0.07f * field * std::sin(status_.elapsed_seconds * 1.4f),
                   status_.elapsed_seconds * 0.18f * direction,
                   radians(12.0f) + 0.04f * field * std::cos(status_.elapsed_seconds * 1.9f)};
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

void LumenRun::updateCaveVisuals(const float dt) {
  auto &objects = scene_.objects();
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
    object.material.emission_strength = 0.055f + ore.hit_flash * 0.120f + damage * 0.030f;
    object.material.edge_wear = 0.18f + damage * 0.38f;
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
  }
}

} // namespace aster
