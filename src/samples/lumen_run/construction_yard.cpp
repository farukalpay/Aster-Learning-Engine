// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "lumen_run_detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace aster {
namespace {

constexpr float kConstructionForkliftInteractDistance = 2.55f;
constexpr float kConstructionPalletAttachDistance = 1.35f;
constexpr float kConstructionShredderFeedDistance = 1.55f;
constexpr float kConstructionForkliftSpeed = 2.45f;
constexpr float kConstructionForkliftTurnRate = 1.45f;
constexpr float kConstructionForkRaiseRate = 0.86f;
constexpr float kConstructionForkliftWheelRadius = 0.255f;
constexpr float kConstructionForkliftMaxSteerAngle = 0.50f;
constexpr int kConstructionPipeCount = 8;

[[nodiscard]] Vec3 constructionForward(const float yaw) {
  return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

[[nodiscard]] Vec3 constructionRotateYaw(const Vec3 value, const float yaw) {
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

[[nodiscard]] Vec3 forkliftSeatPosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({0.0f, 1.18f, -0.28f}, yaw);
}

[[nodiscard]] Vec3 forkliftForkPocketPosition(const Vec3 position, const float yaw,
                                              const float fork_height) {
  return position + constructionRotateYaw({0.0f, fork_height + 0.02f, 1.88f}, yaw);
}

[[nodiscard]] Vec3 forkliftCargoPosition(const Vec3 position, const float yaw,
                                         const float fork_height) {
  return position + constructionRotateYaw({0.0f, fork_height + 0.12f, 2.48f}, yaw);
}

[[nodiscard]] Vec3 shredderIntakePosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({0.0f, 1.02f, -1.12f}, yaw);
}

[[nodiscard]] Vec3 shredderOutputPosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({0.0f, 0.68f, 1.96f}, yaw);
}

[[nodiscard]] float planarDistance(const Vec3 a, const Vec3 b) {
  return length(Vec2{a.x - b.x, a.z - b.z});
}

[[nodiscard]] bool roleContains(const std::string_view role, const std::string_view token) {
  return role.find(token) != std::string_view::npos;
}

void hideObject(RenderObject &object) {
  object.transform.position = {0.0f, -24.0f, 0.0f};
  object.transform.scale = {0.001f, 0.001f, 0.001f};
  object.material.emission_strength = 0.0f;
}

void placePart(RenderObject &object, const Vec3 base_position, const float base_yaw,
               const Vec3 local_position, const Vec3 local_rotation, const Vec3 part_scale,
               const Vec3 local_offset = {},
               const Vec3 extra_rotation = {}, const Vec3 scale_factor = {1.0f, 1.0f, 1.0f}) {
  object.transform.position =
      base_position + constructionRotateYaw(local_position + local_offset, base_yaw);
  object.transform.rotation = quatFromEulerXyz(
      {local_rotation.x + extra_rotation.x, base_yaw + local_rotation.y + extra_rotation.y,
       local_rotation.z + extra_rotation.z});
  object.transform.scale =
      {part_scale.x * scale_factor.x, part_scale.y * scale_factor.y, part_scale.z * scale_factor.z};
}

} // namespace

void LumenRun::toggleConstructionForkliftMount() {
  const Vec3 seat =
      forkliftSeatPosition(construction_forklift_.position, construction_forklift_.yaw);
  if (construction_forklift_.mounted) {
    construction_forklift_.mounted = false;
    player_avatar_animator_.seated_blend = 0.0f;
    player_avatar_pose_.seated_blend = 0.0f;
    player_velocity_ = {};
    const Vec3 exit_probe =
        construction_forklift_.position + constructionRotateYaw({-0.98f, 0.0f, -0.22f},
                                                                construction_forklift_.yaw);
    const TerrainSurfaceSample ground =
        sampleWorldSupport({{exit_probe.x, exit_probe.z}, exit_probe.y + 1.0f, 1.4f, 3.0f});
    player_position_ = {exit_probe.x,
                        (ground.valid ? ground.height : construction_forklift_.position.y) +
                            playerSupportExtent(),
                        exit_probe.z};
    player_facing_yaw_ = construction_forklift_.yaw - kPi * 0.5f;
    if (physics_.valid(player_body_)) {
      physics_.setPosition(player_body_, player_position_);
      physics_.setVelocity(player_body_, {});
    }
    return;
  }

  if (planarDistance(player_position_, seat) > kConstructionForkliftInteractDistance) {
    return;
  }
  construction_forklift_.mounted = true;
  player_avatar_animator_.seated_blend = 1.0f;
  player_avatar_pose_.seated_blend = 1.0f;
  player_velocity_ = {};
  player_position_ = seat;
  player_facing_yaw_ = construction_forklift_.yaw;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  closeChest();
  clearAvatarPointTarget();
}

bool LumenRun::tryAttachConstructionPallet() {
  if (!construction_forklift_.mounted || construction_pallet_.attached ||
      construction_pallet_.consumed || construction_pallet_.attach_cooldown > 0.0f) {
    return false;
  }
  const float alignment =
      planarDistance(forkliftForkPocketPosition(construction_forklift_.position,
                                                construction_forklift_.yaw,
                                                construction_forklift_.fork_height),
                     construction_pallet_.position);
  if (alignment > kConstructionPalletAttachDistance) {
    return false;
  }
  if (construction_forklift_.fork_height < 0.18f ||
      construction_forklift_.fork_height > 0.82f) {
    return false;
  }
  construction_pallet_.attached = true;
  construction_pallet_.yaw = construction_forklift_.yaw;
  construction_pallet_.position =
      forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                            construction_forklift_.fork_height);
  return true;
}

void LumenRun::dropConstructionPallet() {
  if (!construction_pallet_.attached || construction_pallet_.consumed) {
    return;
  }
  construction_pallet_.attached = false;
  construction_pallet_.attach_cooldown = 0.45f;
  construction_pallet_.yaw = construction_forklift_.yaw;
  const Vec3 drop = construction_pallet_.position;
  const TerrainSurfaceSample ground =
      sampleWorldSupport({{drop.x, drop.z}, drop.y + 1.0f, 1.4f, 3.0f});
  construction_pallet_.position = {drop.x, ground.valid ? ground.height : drop.y, drop.z};
}

bool LumenRun::triggerConstructionShredder() {
  if (!construction_pallet_.attached || construction_pallet_.consumed ||
      construction_shredder_.active) {
    return false;
  }
  const float distance =
      planarDistance(construction_pallet_.position,
                     shredderIntakePosition(construction_shredder_.position,
                                            construction_shredder_.yaw));
  if (distance > kConstructionShredderFeedDistance) {
    return false;
  }
  construction_shredder_.active = true;
  construction_shredder_.shred_timer = 0.0f;
  construction_shredder_.consumed_pipe_count = 0;
  construction_pallet_.attached = false;
  construction_pallet_.visible_pipe_count = kConstructionPipeCount;
  return true;
}

void LumenRun::spawnConstructionScrapBurst(const Vec3 center, const int count) {
  if (construction_scrap_.empty() || count <= 0) {
    return;
  }
  const Vec3 output_forward = constructionForward(construction_shredder_.yaw);
  for (int i = 0; i < count; ++i) {
    ConstructionScrapVisual &scrap =
        construction_scrap_[construction_scrap_cursor_ % construction_scrap_.size()];
    ++construction_scrap_cursor_;
    const float phase = status_.elapsed_seconds * 6.7f + static_cast<float>(i) * 1.31f;
    const float side = std::sin(phase) * 0.55f;
    scrap.position = center + Vec3{std::cos(phase) * 0.16f, 0.12f + 0.026f * i,
                                   std::sin(phase * 0.7f) * 0.10f};
    scrap.velocity = output_forward * (1.45f + 0.13f * static_cast<float>(i % 4)) +
                     constructionRotateYaw({side, 0.62f + 0.08f * static_cast<float>(i % 3),
                                            0.20f},
                                           construction_shredder_.yaw);
    scrap.rotation = {phase * 0.3f, phase * 0.7f, phase};
    scrap.angular_velocity = {2.6f + 0.3f * i, 4.2f - 0.2f * i, 3.5f + 0.17f * i};
    scrap.age = 0.0f;
    scrap.lifetime = 1.55f + 0.12f * static_cast<float>(i % 5);
    scrap.base_scale = 0.82f + 0.10f * static_cast<float>(i % 4);
    scrap.active = true;
  }
}

void LumenRun::updateConstructionYard(const float dt, Vec2 move_axis, const bool run_requested,
                                      const bool fork_up) {
  construction_pallet_.attach_cooldown =
      std::max(0.0f, construction_pallet_.attach_cooldown - dt);
  if (construction_forklift_.mounted) {
    if (length(move_axis) > 1.0f) {
      move_axis = normalize(move_axis);
    }
    const float steer_target = move_axis.x * kConstructionForkliftMaxSteerAngle;
    const float steer_blend = std::clamp(dt * 8.0f, 0.0f, 1.0f);
    construction_forklift_.steer_angle +=
        (steer_target - construction_forklift_.steer_angle) * steer_blend;
    const float drive = move_axis.y * kConstructionForkliftSpeed;
    const float drive_sign = drive < -0.001f ? -1.0f : 1.0f;
    const float speed_blend = std::clamp(std::abs(drive) / kConstructionForkliftSpeed, 0.0f, 1.0f);
    construction_forklift_.yaw +=
        construction_forklift_.steer_angle * drive_sign *
        (0.22f + speed_blend * 0.78f) * kConstructionForkliftTurnRate * dt;
    const Vec3 previous = construction_forklift_.position;
    Vec3 next = construction_forklift_.position + constructionForward(construction_forklift_.yaw) *
                                                   (drive * dt);

    const Vec3 shredder = construction_shredder_.position;
    const float shredder_clearance = planarDistance(next, shredder);
    if (shredder_clearance < 1.26f) {
      next = construction_forklift_.position;
    }
    const TerrainSurfaceSample ground =
        sampleWorldSupport({{next.x, next.z}, next.y + 1.0f, 1.4f, 3.0f});
    if (ground.valid) {
      next.y = ground.height + 0.005f;
    }
    construction_forklift_.position = next;
    construction_forklift_.wheel_spin += drive * dt / kConstructionForkliftWheelRadius;
    if (fork_up) {
      construction_forklift_.fork_height += kConstructionForkRaiseRate * dt;
    } else if (run_requested) {
      construction_forklift_.fork_height -= kConstructionForkRaiseRate * dt;
    }
    construction_forklift_.fork_height =
        std::clamp(construction_forklift_.fork_height, 0.22f, 1.05f);

    if (!construction_pallet_.attached) {
      (void)tryAttachConstructionPallet();
    }
    if (construction_pallet_.attached) {
      construction_pallet_.position =
          forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                                construction_forklift_.fork_height);
      construction_pallet_.yaw = construction_forklift_.yaw;
      (void)triggerConstructionShredder();
    }
    player_position_ =
        forkliftSeatPosition(construction_forklift_.position, construction_forklift_.yaw);
    player_velocity_ = dt > 0.0f ? (construction_forklift_.position - previous) / dt : Vec3{};
    player_facing_yaw_ = construction_forklift_.yaw;
    if (physics_.valid(player_body_)) {
      physics_.setPosition(player_body_, player_position_);
      physics_.setVelocity(player_body_, player_velocity_);
    }
  }

  if (construction_shredder_.active) {
    construction_shredder_.shred_timer += dt;
    const Vec3 intake =
        shredderIntakePosition(construction_shredder_.position, construction_shredder_.yaw);
    construction_pallet_.position =
        construction_pallet_.position +
        (intake - construction_pallet_.position) * std::clamp(dt * 2.4f, 0.0f, 1.0f);
    construction_pallet_.yaw = construction_shredder_.yaw;
    const int expected =
        std::min(kConstructionPipeCount,
                 static_cast<int>(std::floor(construction_shredder_.shred_timer / 0.28f)) + 1);
    if (expected > construction_shredder_.consumed_pipe_count) {
      const int burst_count = (expected - construction_shredder_.consumed_pipe_count) * 7;
      construction_shredder_.consumed_pipe_count = expected;
      construction_pallet_.visible_pipe_count = kConstructionPipeCount - expected;
      spawnConstructionScrapBurst(
          shredderOutputPosition(construction_shredder_.position, construction_shredder_.yaw),
          burst_count);
    }
    if (construction_shredder_.consumed_pipe_count >= kConstructionPipeCount &&
        construction_shredder_.shred_timer > 2.55f) {
      construction_shredder_.active = false;
      construction_pallet_.consumed = true;
      construction_pallet_.visible_pipe_count = 0;
    }
  }

  for (ConstructionScrapVisual &scrap : construction_scrap_) {
    if (!scrap.active) {
      continue;
    }
    scrap.age += dt;
    if (scrap.age >= scrap.lifetime) {
      scrap.active = false;
      continue;
    }
    scrap.velocity.y -= 1.95f * dt;
    scrap.position = scrap.position + scrap.velocity * dt;
    scrap.rotation = scrap.rotation + scrap.angular_velocity * dt;
  }
}

void LumenRun::updateConstructionYardVisuals(const float dt) {
  (void)dt;
  auto &objects = scene_.objects();

  for (const ConstructionVisualPart &part : construction_forklift_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    Vec3 offset{};
    Vec3 rotation{};
    if (roleContains(part.role, "fork") || roleContains(part.role, "carriage")) {
      offset.y += construction_forklift_.fork_height - 0.28f;
    }
    if (roleContains(part.role, "wheel")) {
      rotation.x += construction_forklift_.wheel_spin;
      if (roleContains(part.role, "rear wheel")) {
        rotation.y += construction_forklift_.steer_angle;
      }
    }
    placePart(objects[part.object_index], construction_forklift_.position, construction_forklift_.yaw,
              part.local_position, part.local_rotation, part.scale, offset, rotation);
  }

  for (const ConstructionVisualPart &part : construction_pallet_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[part.object_index];
    if (construction_pallet_.consumed ||
        (roleContains(part.role, "pipe") && construction_pallet_.visible_pipe_count <= 0)) {
      hideObject(object);
      continue;
    }
    if (roleContains(part.role, "pipe")) {
      int pipe_index = 0;
      const std::size_t suffix = part.role.find_last_of(' ');
      if (suffix != std::string::npos) {
        pipe_index = std::max(0, std::atoi(part.role.c_str() + suffix + 1u));
      }
      if (pipe_index >= construction_pallet_.visible_pipe_count) {
        hideObject(object);
        continue;
      }
      const float feed_scale =
          construction_shredder_.active
              ? std::clamp(1.0f - construction_shredder_.shred_timer * 0.18f, 0.28f, 1.0f)
              : 1.0f;
      placePart(object, construction_pallet_.position, construction_pallet_.yaw,
                part.local_position, part.local_rotation, part.scale, {}, {0.0f, 0.0f, 0.0f},
                {1.0f, feed_scale, 1.0f});
    } else {
      placePart(object, construction_pallet_.position, construction_pallet_.yaw,
                part.local_position, part.local_rotation, part.scale);
    }
  }

  for (const ConstructionVisualPart &part : construction_shredder_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    Vec3 rotation{};
    if (roleContains(part.role, "tooth")) {
      const float spin = construction_shredder_.active
                             ? status_.elapsed_seconds * 12.0f +
                                   static_cast<float>(part.object_index % 7u) * 0.55f
                             : std::sin(status_.elapsed_seconds * 0.8f) * 0.06f;
      rotation.z += spin;
      objects[part.object_index].material.emission_strength =
          construction_shredder_.active ? 0.10f : 0.0f;
    }
    placePart(objects[part.object_index], construction_shredder_.position,
              construction_shredder_.yaw, part.local_position, part.local_rotation, part.scale, {},
              rotation);
  }

  for (ConstructionScrapVisual &scrap : construction_scrap_) {
    if (scrap.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[scrap.object_index];
    if (!scrap.active) {
      hideObject(object);
      continue;
    }
    const float fade = 1.0f - std::clamp(scrap.age / std::max(scrap.lifetime, 0.001f), 0.0f, 1.0f);
    object.transform.position = scrap.position;
    object.transform.rotation = quatFromEulerXyz(scrap.rotation);
    const float scale = scrap.base_scale * (0.105f + fade * 0.105f);
    object.transform.scale = {scale, scale, scale};
    object.material.emission_strength = 0.18f + fade * 0.58f;
  }
}

} // namespace aster
