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
constexpr int kConstructionLightLoadCount = 16;
constexpr float kConstructionImpactDistance = 1.42f;
constexpr float kConstructionShredSeconds = 1.82f;
const std::array<Vec3, 4> kConstructionForkliftWheelCenters = {
    Vec3{-0.66f, 0.29f, -0.78f},
    Vec3{0.66f, 0.29f, -0.78f},
    Vec3{-0.66f, 0.24f, 0.78f},
    Vec3{0.66f, 0.24f, 0.78f},
};
const std::array<float, 4> kConstructionForkliftWheelVisualRadii = {
    0.50f * 0.56f,
    0.50f * 0.56f,
    0.50f * 0.46f,
    0.50f * 0.46f,
};

[[nodiscard]] Vec3 constructionForward(const float yaw) {
  return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

[[nodiscard]] Vec3 constructionRotateX(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x, value.y * c - value.z * s, value.y * s + value.z * c};
}

[[nodiscard]] Vec3 constructionRotateYaw(const Vec3 value, const float yaw) {
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

[[nodiscard]] Vec3 constructionRotateZ(const Vec3 value, const float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

[[nodiscard]] Vec3 constructionRotateAttitude(const Vec3 value, const float yaw,
                                              const float pitch, const float roll) {
  return constructionRotateZ(constructionRotateYaw(constructionRotateX(value, pitch), yaw), roll);
}

[[nodiscard]] Vec3 forkliftSeatPosition(const Vec3 position, const float yaw,
                                        const float pitch = 0.0f, const float roll = 0.0f) {
  return position + constructionRotateAttitude({0.0f, 1.18f, -0.28f}, yaw, pitch, roll);
}

[[nodiscard]] Vec3 forkliftForkPocketPosition(const Vec3 position, const float yaw,
                                              const float fork_height,
                                              const float pitch = 0.0f,
                                              const float roll = 0.0f) {
  return position +
         constructionRotateAttitude({0.0f, fork_height + 0.02f, 1.88f}, yaw, pitch, roll);
}

[[nodiscard]] Vec3 forkliftCargoPosition(const Vec3 position, const float yaw,
                                         const float fork_height, const float pitch = 0.0f,
                                         const float roll = 0.0f) {
  return position +
         constructionRotateAttitude({0.0f, fork_height + 0.12f, 2.48f}, yaw, pitch, roll);
}

[[nodiscard]] Vec3 shredderIntakePosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({0.0f, 1.02f, -1.12f}, yaw);
}

[[nodiscard]] Vec3 shredderOutputPosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({0.0f, 0.72f, 2.64f}, yaw);
}

[[nodiscard]] Vec3 craneSeatPosition(const Vec3 position, const float yaw) {
  return position + constructionRotateYaw({-0.58f, 1.42f, 0.82f}, yaw);
}

[[nodiscard]] Vec3 craneHookPosition(const Vec3 position, const float yaw, const float boom_yaw,
                                     const float hook_height, const float hook_reach) {
  return position +
         constructionRotateYaw({0.0f, hook_height, -hook_reach}, yaw + boom_yaw);
}

[[nodiscard]] int pressStrokeRequirement(const int pending_load_count) {
  if (pending_load_count <= 2) {
    return 3;
  }
  return pending_load_count == 3 ? 4 : 5;
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
               const float base_pitch, const float base_roll, const Vec3 local_position,
               const Vec3 local_rotation, const Vec3 part_scale,
               const Vec3 local_offset = {},
               const Vec3 extra_rotation = {}, const Vec3 scale_factor = {1.0f, 1.0f, 1.0f}) {
  object.transform.position =
      base_position +
      constructionRotateAttitude(local_position + local_offset, base_yaw, base_pitch, base_roll);
  object.transform.rotation = quatFromEulerXyz(
      {base_pitch + local_rotation.x + extra_rotation.x,
       base_yaw + local_rotation.y + extra_rotation.y,
       base_roll + local_rotation.z + extra_rotation.z});
  object.transform.scale =
      {part_scale.x * scale_factor.x, part_scale.y * scale_factor.y, part_scale.z * scale_factor.z};
}

} // namespace

void LumenRun::toggleConstructionForkliftMount() {
  const Vec3 seat =
      forkliftSeatPosition(construction_forklift_.position, construction_forklift_.yaw,
                           construction_forklift_.pitch, construction_forklift_.roll);
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
    pushSandboxLog("yard: forklift parked");
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
  pushSandboxLog("yard: forklift mounted");
}

void LumenRun::toggleConstructionCraneMount() {
  const Vec3 seat = craneSeatPosition(construction_crane_.position, construction_crane_.yaw);
  if (construction_crane_.mounted) {
    const Vec3 hook = craneHookPosition(construction_crane_.position, construction_crane_.yaw,
                                        construction_crane_.boom_yaw,
                                        construction_crane_.hook_height,
                                        construction_crane_.hook_reach);
    if (construction_crane_.payload_module_index >= 0 &&
        static_cast<std::size_t>(construction_crane_.payload_module_index) <
            construction_modules_.size()) {
      ConstructionDemolitionModule &module =
          construction_modules_[static_cast<std::size_t>(construction_crane_.payload_module_index)];
      const Vec3 intake =
          shredderIntakePosition(construction_shredder_.position, construction_shredder_.yaw);
      if (!construction_shredder_.active && construction_press_.pending_load_count < 4 &&
          planarDistance(hook, intake) <= 2.20f && std::abs(hook.y - intake.y) <= 1.60f) {
        module.carried = false;
        module.processing = true;
        construction_shredder_.active = true;
        construction_shredder_.shred_timer = 0.0f;
        construction_shredder_.processing_module_index = construction_crane_.payload_module_index;
        construction_shredder_.processing_crane_load = true;
        spawnConstructionScrapBurst(
            shredderOutputPosition(construction_shredder_.position, construction_shredder_.yaw),
            12);
        pushSandboxLog("yard: crane fed heavy beam to shredder");
      } else {
        module.carried = false;
        module.position = hook;
      }
      construction_crane_.payload_module_index = -1;
      return;
    }
    if (!construction_shredder_.active && construction_press_.pending_load_count < 4) {
      for (std::size_t i = 0; i < construction_modules_.size(); ++i) {
        ConstructionDemolitionModule &module = construction_modules_[i];
        if (!module.heavy || !module.unlocked || module.processed || module.processing ||
            module.carried || planarDistance(hook, module.position) > 1.84f ||
            std::abs(hook.y - module.position.y) > 1.60f) {
          continue;
        }
        module.detached = true;
        module.carried = true;
        construction_crane_.payload_module_index = static_cast<int>(i);
        pushSandboxLog("yard: crane lifted heavy beam");
        return;
      }
    }
    construction_crane_.mounted = false;
    player_avatar_animator_.seated_blend = 0.0f;
    player_avatar_pose_.seated_blend = 0.0f;
    const Vec3 exit_probe =
        construction_crane_.position + constructionRotateYaw({-1.92f, 0.0f, 0.84f},
                                                              construction_crane_.yaw);
    const TerrainSurfaceSample ground =
        sampleWorldSupport({{exit_probe.x, exit_probe.z}, exit_probe.y + 1.0f, 1.4f, 3.0f});
    player_position_ = {exit_probe.x,
                        (ground.valid ? ground.height : construction_crane_.position.y) +
                            playerSupportExtent(),
                        exit_probe.z};
    player_velocity_ = {};
    pushSandboxLog("yard: crane parked");
    return;
  }
  if (planarDistance(player_position_, seat) > kConstructionForkliftInteractDistance) {
    return;
  }
  if (construction_forklift_.mounted) {
    toggleConstructionForkliftMount();
  }
  construction_crane_.mounted = true;
  player_avatar_animator_.seated_blend = 1.0f;
  player_avatar_pose_.seated_blend = 1.0f;
  player_position_ = seat;
  player_velocity_ = {};
  player_facing_yaw_ = construction_crane_.yaw;
  if (physics_.valid(player_body_)) {
    physics_.setPosition(player_body_, player_position_);
    physics_.setVelocity(player_body_, {});
  }
  pushSandboxLog("yard: crane mounted");
}

bool LumenRun::tryAttachConstructionPallet() {
  if (!construction_forklift_.mounted || construction_pallet_.attached ||
      construction_pallet_.cargo_kind != ConstructionCargoKind::None ||
      construction_pallet_.attach_cooldown > 0.0f) {
    return false;
  }
  const Vec3 fork =
      forkliftForkPocketPosition(construction_forklift_.position, construction_forklift_.yaw,
                                 construction_forklift_.fork_height, construction_forklift_.pitch,
                                 construction_forklift_.roll);
  for (std::size_t i = 0; i < construction_bales_.size(); ++i) {
    ConstructionBale &bale = construction_bales_[i];
    if (!bale.available || bale.attached || bale.delivered ||
        planarDistance(fork, bale.position) > kConstructionPalletAttachDistance) {
      continue;
    }
    bale.attached = true;
    construction_pallet_.attached = true;
    construction_pallet_.consumed = false;
    construction_pallet_.cargo_kind = ConstructionCargoKind::Bale;
    construction_pallet_.payload_index = static_cast<int>(i);
    construction_pallet_.yaw = construction_forklift_.yaw;
    bale.yaw = construction_forklift_.yaw;
    construction_pallet_.position =
        forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                              construction_forklift_.fork_height, construction_forklift_.pitch,
                              construction_forklift_.roll);
    pushSandboxLog("yard: bale loaded on forks");
    return true;
  }
  for (std::size_t i = 0; i < construction_modules_.size(); ++i) {
    ConstructionDemolitionModule &module = construction_modules_[i];
    if (module.heavy || !module.detached || module.carried || module.falling ||
        module.processing || module.processed ||
        planarDistance(fork, module.position) > kConstructionPalletAttachDistance) {
      continue;
    }
    module.carried = true;
    module.fall_velocity = {};
    module.yaw = construction_forklift_.yaw;
    construction_pallet_.attached = true;
    construction_pallet_.consumed = false;
    construction_pallet_.cargo_kind = ConstructionCargoKind::LightModule;
    construction_pallet_.payload_index = static_cast<int>(i);
    construction_pallet_.yaw = construction_forklift_.yaw;
    construction_pallet_.position =
        forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                              construction_forklift_.fork_height, construction_forklift_.pitch,
                              construction_forklift_.roll);
    pushSandboxLog("yard: light module loaded for shredder");
    return true;
  }
  return false;
}

bool LumenRun::tryAttachConstructionBale() {
  if (construction_pallet_.cargo_kind != ConstructionCargoKind::None) {
    return false;
  }
  return tryAttachConstructionPallet();
}

void LumenRun::deliverConstructionBale() {
  if (!construction_pallet_.attached ||
      construction_pallet_.cargo_kind != ConstructionCargoKind::Bale ||
      construction_pallet_.payload_index < 0 ||
      static_cast<std::size_t>(construction_pallet_.payload_index) >= construction_bales_.size()) {
    return;
  }
  if (planarDistance(construction_pallet_.position, construction_delivery_rack_.position) > 3.45f) {
    return;
  }
  ConstructionBale &bale =
      construction_bales_[static_cast<std::size_t>(construction_pallet_.payload_index)];
  const int delivery_index = constructionDeliveredBaleCount();
  const int column = delivery_index % 4;
  const int row = delivery_index / 4;
  bale.position = construction_delivery_rack_.position +
                  Vec3{-2.22f + static_cast<float>(column) * 1.45f,
                       0.24f + static_cast<float>(row) * 0.76f,
                       -1.08f + static_cast<float>((row / 2) % 2) * 1.32f};
  bale.attached = false;
  bale.delivered = true;
  construction_pallet_.attached = false;
  construction_pallet_.consumed = true;
  construction_pallet_.cargo_kind = ConstructionCargoKind::None;
  construction_pallet_.payload_index = -1;
  construction_pallet_.attach_cooldown = 0.35f;
  pushSandboxLog("yard: bale delivered to rack");
}

void LumenRun::dropConstructionPallet() {
  if (!construction_pallet_.attached || construction_pallet_.cargo_kind == ConstructionCargoKind::None) {
    return;
  }
  if (construction_pallet_.cargo_kind == ConstructionCargoKind::Bale) {
    deliverConstructionBale();
    if (!construction_pallet_.attached) {
      return;
    }
  }
  const Vec3 drop = construction_pallet_.position;
  const TerrainSurfaceSample ground =
      sampleWorldSupport({{drop.x, drop.z}, drop.y + 1.0f, 1.4f, 3.0f});
  const float ground_height = ground.valid ? ground.height : drop.y;
  const Vec3 grounded{drop.x, ground_height, drop.z};
  if (construction_pallet_.payload_index >= 0 &&
      construction_pallet_.cargo_kind == ConstructionCargoKind::LightModule &&
      static_cast<std::size_t>(construction_pallet_.payload_index) < construction_modules_.size()) {
    ConstructionDemolitionModule &module =
        construction_modules_[static_cast<std::size_t>(construction_pallet_.payload_index)];
    float half_height = 0.0f;
    for (const ConstructionVisualPart &part : module.parts) {
      half_height = std::max(half_height, part.local_position.y + part.scale.y * 0.5f);
    }
    module.position = {drop.x, ground_height + std::max(half_height, 1.0f) + 0.018f, drop.z};
    module.carried = false;
    module.falling = false;
    module.fall_velocity = {};
    module.yaw = construction_pallet_.yaw;
  } else if (construction_pallet_.payload_index >= 0 &&
             construction_pallet_.cargo_kind == ConstructionCargoKind::Bale &&
             static_cast<std::size_t>(construction_pallet_.payload_index) < construction_bales_.size()) {
    ConstructionBale &bale =
        construction_bales_[static_cast<std::size_t>(construction_pallet_.payload_index)];
    bale.position = grounded;
    bale.yaw = construction_pallet_.yaw;
    bale.attached = false;
  }
  construction_pallet_.position = grounded;
  construction_pallet_.attached = false;
  construction_pallet_.consumed = true;
  construction_pallet_.cargo_kind = ConstructionCargoKind::None;
  construction_pallet_.payload_index = -1;
  construction_pallet_.attach_cooldown = 0.45f;
}

bool LumenRun::triggerConstructionShredder() {
  if (!construction_pallet_.attached ||
      construction_pallet_.cargo_kind != ConstructionCargoKind::LightModule ||
      construction_pallet_.payload_index < 0 || construction_shredder_.active ||
      construction_press_.pending_load_count >= 4) {
    return false;
  }
  const float alignment =
      planarDistance(construction_pallet_.position,
                     shredderIntakePosition(construction_shredder_.position,
                                            construction_shredder_.yaw));
  if (alignment > kConstructionShredderFeedDistance + 0.52f ||
      static_cast<std::size_t>(construction_pallet_.payload_index) >= construction_modules_.size()) {
    return false;
  }
  ConstructionDemolitionModule &module =
      construction_modules_[static_cast<std::size_t>(construction_pallet_.payload_index)];
  construction_shredder_.active = true;
  construction_shredder_.shred_timer = 0.0f;
  construction_shredder_.processing_module_index = construction_pallet_.payload_index;
  construction_shredder_.processing_crane_load = false;
  module.carried = false;
  module.processing = true;
  construction_pallet_.attached = false;
  construction_pallet_.consumed = true;
  construction_pallet_.cargo_kind = ConstructionCargoKind::None;
  construction_pallet_.payload_index = -1;
  construction_pallet_.pitch = 0.0f;
  construction_pallet_.roll = 0.0f;
  spawnConstructionScrapBurst(
      shredderOutputPosition(construction_shredder_.position, construction_shredder_.yaw), 9);
  pushSandboxLog("yard: shredder intake locked, sparks active");
  return true;
}

bool LumenRun::triggerConstructionPressStroke() {
  if (construction_press_.pending_load_count <= 0) {
    return false;
  }
  if (construction_press_.required_strokes <= 0) {
    construction_press_.required_strokes =
        pressStrokeRequirement(construction_press_.pending_load_count);
  }
  ++construction_press_.stroke_count;
  construction_press_.stroke_animation = 1.0f;
  spawnConstructionScrapBurst(
      construction_press_.position + constructionRotateYaw({0.0f, 0.62f, 1.08f},
                                                            construction_press_.yaw),
      4);
  if (construction_press_.stroke_count < construction_press_.required_strokes) {
    pushSandboxLog("yard: press stroke " + std::to_string(construction_press_.stroke_count) + "/" +
                   std::to_string(construction_press_.required_strokes));
    return true;
  }
  const auto available = std::find_if(
      construction_bales_.begin(), construction_bales_.end(),
      [](const ConstructionBale &bale) { return !bale.available && !bale.delivered; });
  if (available != construction_bales_.end()) {
    available->available = true;
    available->load_count = construction_press_.pending_load_count;
    available->position =
        construction_press_.position + constructionRotateYaw({0.0f, 0.0f, 2.72f},
                                                              construction_press_.yaw);
    available->yaw = construction_press_.yaw;
  }
  construction_press_.pending_load_count = 0;
  construction_press_.stroke_count = 0;
  construction_press_.required_strokes = 0;
  pushSandboxLog("yard: scrap compacted into a bale");
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

LumenRun::ConstructionForkliftSupportPose
LumenRun::constructionForkliftSupportPoseAt(const Vec3 position, const float yaw,
                                            const float pitch, const float roll) const {
  ConstructionForkliftSupportPose pose;
  pose.position = position;
  pose.pitch = pitch;
  pose.roll = roll;

  const Vec3 forward = constructionForward(yaw);
  const Vec3 side = constructionRotateYaw({1.0f, 0.0f, 0.0f}, yaw);
  const auto sample_wheel_support = [&](const Vec3 probe, const float reference_y) {
    constexpr std::array<Vec2, 5> offsets = {
        Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec2{-1.0f, 0.0f}, Vec2{0.0f, 1.0f},
        Vec2{0.0f, -1.0f}};
    constexpr std::array<float, 5> weights = {1.0f, 0.48f, 0.48f, 0.48f, 0.48f};
    TerrainSurfaceSample blended{};
    float weight_sum = 0.0f;
    float height_sum = 0.0f;
    Vec3 normal_sum{};
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      const Vec3 sample_position =
          probe + side * (offsets[i].x * 0.16f) + forward * (offsets[i].y * 0.16f);
      const TerrainSurfaceSample sample = sampleWorldSupport(
          {{sample_position.x, sample_position.z}, reference_y, 0.34f, 1.20f});
      if (!sample.valid || sample.normal.y < 0.58f) {
        continue;
      }
      const float weight = weights[i];
      weight_sum += weight;
      height_sum += sample.height * weight;
      normal_sum = normal_sum + sample.normal * weight;
    }
    if (weight_sum <= 0.0f) {
      return blended;
    }
    blended.valid = true;
    blended.height = height_sum / weight_sum;
    blended.normal = length(normal_sum) > 0.0001f ? normalize(normal_sum) : Vec3{0.0f, 1.0f, 0.0f};
    return blended;
  };

  std::array<TerrainSurfaceSample, 4> supports{};
  for (std::size_t i = 0; i < kConstructionForkliftWheelCenters.size(); ++i) {
    const Vec3 local = kConstructionForkliftWheelCenters[i];
    const float bottom_y = local.y - kConstructionForkliftWheelVisualRadii[i];
    const Vec3 bottom =
        position + constructionRotateAttitude({local.x, bottom_y, local.z}, yaw, pitch, roll);
    supports[i] = sample_wheel_support(bottom, bottom.y + 0.14f);
    pose.wheel_contacts[i].wheel_center =
        position + constructionRotateAttitude(local, yaw, pitch, roll);
    pose.wheel_contacts[i].contact_point =
        supports[i].valid ? Vec3{bottom.x, supports[i].height, bottom.z} : bottom;
    pose.wheel_contacts[i].normal =
        supports[i].valid ? supports[i].normal : Vec3{0.0f, 1.0f, 0.0f};
    pose.wheel_contacts[i].support_height = supports[i].valid ? supports[i].height : bottom.y;
    pose.wheel_contacts[i].grounded = supports[i].valid;
  }

  if (!std::all_of(supports.begin(), supports.end(),
                   [](const TerrainSurfaceSample &sample) { return sample.valid; })) {
    return pose;
  }

  const float rear_height = (supports[0].height + supports[1].height) * 0.5f;
  const float front_height = (supports[2].height + supports[3].height) * 0.5f;
  const float left_height = (supports[0].height + supports[2].height) * 0.5f;
  const float right_height = (supports[1].height + supports[3].height) * 0.5f;
  const float wheelbase =
      std::max(kConstructionForkliftWheelCenters[2].z - kConstructionForkliftWheelCenters[0].z,
               0.001f);
  const float track =
      std::max(kConstructionForkliftWheelCenters[1].x - kConstructionForkliftWheelCenters[0].x,
               0.001f);
  pose.pitch = std::clamp(-(front_height - rear_height) / wheelbase, -0.18f, 0.18f);
  pose.roll = std::clamp((right_height - left_height) / track, -0.14f, 0.14f);

  float base_y = -std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < kConstructionForkliftWheelCenters.size(); ++i) {
    const Vec3 local = kConstructionForkliftWheelCenters[i];
    const float bottom_y = local.y - kConstructionForkliftWheelVisualRadii[i];
    const float vertical_offset =
        constructionRotateAttitude({local.x, bottom_y, local.z}, yaw, pose.pitch, pose.roll).y;
    base_y = std::max(base_y, supports[i].height + 0.012f - vertical_offset);
  }
  pose.position.y = base_y;
  float lift_correction = 0.0f;
  for (std::size_t i = 0; i < kConstructionForkliftWheelCenters.size(); ++i) {
    const Vec3 local = kConstructionForkliftWheelCenters[i];
    const float bottom_y = local.y - kConstructionForkliftWheelVisualRadii[i];
    const Vec3 bottom =
        pose.position +
        constructionRotateAttitude({local.x, bottom_y, local.z}, yaw, pose.pitch, pose.roll);
    lift_correction = std::max(lift_correction, supports[i].height + 0.008f - bottom.y);
  }
  pose.position.y += std::max(lift_correction, 0.0f);

  for (std::size_t i = 0; i < kConstructionForkliftWheelCenters.size(); ++i) {
    const Vec3 local = kConstructionForkliftWheelCenters[i];
    const float bottom_y = local.y - kConstructionForkliftWheelVisualRadii[i];
    const Vec3 bottom =
        pose.position +
        constructionRotateAttitude({local.x, bottom_y, local.z}, yaw, pose.pitch, pose.roll);
    pose.wheel_contacts[i].wheel_center =
        pose.position + constructionRotateAttitude(local, yaw, pose.pitch, pose.roll);
    pose.wheel_contacts[i].contact_point = {bottom.x, supports[i].height, bottom.z};
    pose.wheel_contacts[i].normal = supports[i].normal;
    pose.wheel_contacts[i].support_height = supports[i].height;
    pose.wheel_contacts[i].grounded = bottom.y >= supports[i].height - 0.002f;
  }
  return pose;
}

void LumenRun::updateConstructionYard(const float dt, Vec2 move_axis, const bool run_requested,
                                      const bool fork_up) {
  construction_pallet_.attach_cooldown =
      std::max(0.0f, construction_pallet_.attach_cooldown - dt);
  construction_press_.stroke_animation =
      std::max(0.0f, construction_press_.stroke_animation - dt * 2.9f);
  for (ConstructionDemolitionModule &module : construction_modules_) {
    module.impact_cooldown = std::max(0.0f, module.impact_cooldown - dt);
  }
  const auto module_half_height = [](const ConstructionDemolitionModule &module) {
    float half_height = 0.0f;
    for (const ConstructionVisualPart &part : module.parts) {
      half_height = std::max(half_height, part.local_position.y + part.scale.y * 0.5f);
    }
    return std::max(half_height, module.heavy ? 0.16f : 1.0f);
  };
  const auto support_removed_under = [&](const ConstructionDemolitionModule &module) {
    if (module.heavy || module.tier <= 0) {
      return false;
    }
    const auto support = std::find_if(
        construction_modules_.begin(), construction_modules_.end(),
        [&](const ConstructionDemolitionModule &candidate) {
          return !candidate.heavy && candidate.tier == module.tier - 1 &&
                 candidate.face == module.face && candidate.bay == module.bay;
        });
    return support == construction_modules_.end() || support->detached || support->carried ||
           support->processing || support->processed;
  };
  if (construction_forklift_.mounted) {
    if (length(move_axis) > 1.0f) {
      move_axis = normalize(move_axis);
    }
    const float steer_target = -move_axis.x * kConstructionForkliftMaxSteerAngle;
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

    const bool machine_blocked =
        planarDistance(next, construction_shredder_.position) < 1.72f ||
        planarDistance(next, construction_press_.position) < 1.84f ||
        planarDistance(next, construction_crane_.position) < 2.10f;
    if (machine_blocked) {
      next = construction_forklift_.position;
    }
    const ConstructionForkliftSupportPose support_pose = constructionForkliftSupportPoseAt(
        next, construction_forklift_.yaw, construction_forklift_.pitch, construction_forklift_.roll);
    construction_forklift_.position = support_pose.position;
    construction_forklift_.pitch = support_pose.pitch;
    construction_forklift_.roll = support_pose.roll;
    construction_forklift_.wheel_contacts = support_pose.wheel_contacts;
    construction_forklift_.wheel_spin += drive * dt / kConstructionForkliftWheelRadius;
    if (fork_up) {
      construction_forklift_.fork_height += kConstructionForkRaiseRate * dt;
    } else if (run_requested) {
      construction_forklift_.fork_height -= kConstructionForkRaiseRate * dt;
    }
    construction_forklift_.fork_height =
        std::clamp(construction_forklift_.fork_height, 0.22f, 1.05f);

    if (drive > 0.32f && construction_pallet_.cargo_kind == ConstructionCargoKind::None) {
      const Vec3 fork_tip =
          forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                                construction_forklift_.fork_height, construction_forklift_.pitch,
                                construction_forklift_.roll);
      for (ConstructionDemolitionModule &module : construction_modules_) {
        if (module.heavy || module.detached || module.processing || module.processed ||
            module.impact_cooldown > 0.0f ||
            planarDistance(fork_tip, module.position) > kConstructionImpactDistance) {
          continue;
        }
        ++module.impact_count;
        module.impact_cooldown = 0.42f;
        spawnConstructionScrapBurst(module.position + Vec3{0.0f, 0.38f, 0.0f}, 3);
        if (module.impact_count >= 3) {
          module.detached = true;
          module.fall_velocity = {};
        }
        break;
      }
    }
    for (ConstructionDemolitionModule &module : construction_modules_) {
      if (!module.heavy && !module.detached && !module.carried && !module.processing &&
          !module.processed && support_removed_under(module)) {
        module.detached = true;
        module.falling = true;
        module.fall_velocity = {0.0f, -0.10f, 0.0f};
        module.impact_cooldown = 0.30f;
      }
    }
    if (!construction_pallet_.attached) {
      (void)tryAttachConstructionPallet();
    }
    if (construction_pallet_.attached) {
      construction_pallet_.position =
          forkliftCargoPosition(construction_forklift_.position, construction_forklift_.yaw,
                                construction_forklift_.fork_height, construction_forklift_.pitch,
                                construction_forklift_.roll);
      construction_pallet_.yaw = construction_forklift_.yaw;
      construction_pallet_.pitch = construction_forklift_.pitch;
      construction_pallet_.roll = construction_forklift_.roll;
      if (construction_pallet_.payload_index >= 0 &&
          construction_pallet_.cargo_kind == ConstructionCargoKind::LightModule &&
          static_cast<std::size_t>(construction_pallet_.payload_index) < construction_modules_.size()) {
        ConstructionDemolitionModule &module =
            construction_modules_[static_cast<std::size_t>(construction_pallet_.payload_index)];
        module.position = construction_pallet_.position;
        module.yaw = construction_pallet_.yaw;
        module.falling = false;
        module.fall_velocity = {};
        (void)triggerConstructionShredder();
      } else if (construction_pallet_.payload_index >= 0 &&
                 construction_pallet_.cargo_kind == ConstructionCargoKind::Bale &&
                 static_cast<std::size_t>(construction_pallet_.payload_index) < construction_bales_.size()) {
        ConstructionBale &bale =
            construction_bales_[static_cast<std::size_t>(construction_pallet_.payload_index)];
        bale.position = construction_pallet_.position;
        bale.yaw = construction_pallet_.yaw;
      }
    }
    player_position_ =
        forkliftSeatPosition(construction_forklift_.position, construction_forklift_.yaw,
                             construction_forklift_.pitch, construction_forklift_.roll);
    player_velocity_ = dt > 0.0f ? (construction_forklift_.position - previous) / dt : Vec3{};
    player_facing_yaw_ = construction_forklift_.yaw;
    if (physics_.valid(player_body_)) {
      physics_.setPosition(player_body_, player_position_);
      physics_.setVelocity(player_body_, player_velocity_);
    }
  } else if (construction_crane_.mounted) {
    if (length(move_axis) > 1.0f) {
      move_axis = normalize(move_axis);
    }
    construction_crane_.boom_yaw =
        std::clamp(construction_crane_.boom_yaw - move_axis.x * dt * 0.72f, -1.24f, 1.24f);
    construction_crane_.hook_reach =
        std::clamp(construction_crane_.hook_reach + move_axis.y * dt * 2.2f, 4.4f, 11.4f);
    if (fork_up) {
      construction_crane_.hook_height = std::min(construction_crane_.hook_height + dt * 2.1f, 6.2f);
    } else if (run_requested) {
      construction_crane_.hook_height = std::max(construction_crane_.hook_height - dt * 2.1f, 0.72f);
    }
    const Vec3 hook = craneHookPosition(construction_crane_.position, construction_crane_.yaw,
                                        construction_crane_.boom_yaw,
                                        construction_crane_.hook_height,
                                        construction_crane_.hook_reach);
    if (construction_crane_.payload_module_index >= 0 &&
        static_cast<std::size_t>(construction_crane_.payload_module_index) < construction_modules_.size()) {
      ConstructionDemolitionModule &module =
          construction_modules_[static_cast<std::size_t>(construction_crane_.payload_module_index)];
      module.position = hook;
    }
    player_position_ = craneSeatPosition(construction_crane_.position, construction_crane_.yaw);
    player_velocity_ = {};
    player_facing_yaw_ = construction_crane_.yaw + construction_crane_.boom_yaw;
    if (physics_.valid(player_body_)) {
      physics_.setPosition(player_body_, player_position_);
      physics_.setVelocity(player_body_, {});
    }
  }

  for (ConstructionDemolitionModule &module : construction_modules_) {
    if (!module.falling || module.carried || module.processing || module.processed) {
      continue;
    }
    const float half_height = module_half_height(module);
    const TerrainSurfaceSample ground = sampleWorldSupport(
        {{module.position.x, module.position.z}, module.position.y + half_height + 0.50f,
         0.80f, 8.0f});
    const float target_y = (ground.valid ? ground.height : module.position.y - half_height) +
                           half_height + 0.018f;
    module.fall_velocity.y -= 7.4f * dt;
    module.position = module.position + module.fall_velocity * dt;
    module.yaw += (module.face == 0 ? 0.18f : -0.18f) * dt;
    if (module.position.y <= target_y) {
      module.position.y = target_y;
      module.falling = false;
      module.fall_velocity = {};
      spawnConstructionScrapBurst(module.position - Vec3{0.0f, half_height * 0.42f, 0.0f}, 2);
    }
  }

  if (construction_shredder_.active) {
    construction_shredder_.shred_timer += dt;
    const Vec3 intake =
        shredderIntakePosition(construction_shredder_.position, construction_shredder_.yaw);
    if (construction_shredder_.processing_module_index >= 0 &&
        static_cast<std::size_t>(construction_shredder_.processing_module_index) <
            construction_modules_.size()) {
      ConstructionDemolitionModule &module =
          construction_modules_[static_cast<std::size_t>(construction_shredder_.processing_module_index)];
      const float blend = std::clamp(dt * 2.8f, 0.0f, 1.0f);
      module.position = module.position + (intake - module.position) * blend;
    }
    const int previous_stage =
        static_cast<int>(std::floor(std::max(construction_shredder_.shred_timer - dt, 0.0f) / 0.28f));
    const int current_stage =
        static_cast<int>(std::floor(construction_shredder_.shred_timer / 0.28f));
    if (current_stage > previous_stage) {
      spawnConstructionScrapBurst(
          shredderOutputPosition(construction_shredder_.position, construction_shredder_.yaw),
          construction_shredder_.processing_crane_load ? 11 : 7);
    }
    if (construction_shredder_.shred_timer > kConstructionShredSeconds) {
      if (construction_shredder_.processing_module_index >= 0 &&
          static_cast<std::size_t>(construction_shredder_.processing_module_index) <
              construction_modules_.size()) {
        ConstructionDemolitionModule &module =
            construction_modules_[static_cast<std::size_t>(construction_shredder_.processing_module_index)];
        module.processing = false;
        module.processed = true;
      }
      construction_shredder_.active = false;
      ++construction_shredder_.processed_load_count;
      construction_shredder_.consumed_pipe_count = construction_shredder_.processed_load_count;
      construction_press_.pending_load_count =
          std::min(construction_press_.pending_load_count + 1, 4);
      construction_shredder_.processing_module_index = -1;
      construction_shredder_.processing_crane_load = false;
      pushSandboxLog("yard: scrap load ready for press (" +
                     std::to_string(construction_press_.pending_load_count) + "/4)");
    }
  }

  const int processed_light = static_cast<int>(std::count_if(
      construction_modules_.begin(), construction_modules_.end(),
      [](const ConstructionDemolitionModule &module) { return !module.heavy && module.processed; }));
  if (processed_light >= kConstructionLightLoadCount) {
    for (ConstructionDemolitionModule &module : construction_modules_) {
      if (module.heavy) {
        module.unlocked = true;
      }
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
              construction_forklift_.pitch, construction_forklift_.roll, part.local_position,
              part.local_rotation, part.scale, offset, rotation);
  }

  for (const ConstructionDemolitionModule &module : construction_modules_) {
    for (const ConstructionVisualPart &part : module.parts) {
      if (part.object_index >= objects.size()) {
        continue;
      }
      RenderObject &object = objects[part.object_index];
      if (module.processed) {
        hideObject(object);
        continue;
      }
      const float feed_scale =
          module.processing
              ? std::clamp(1.0f - construction_shredder_.shred_timer / kConstructionShredSeconds,
                           0.08f, 1.0f)
              : 1.0f;
      placePart(object, module.position, module.yaw, 0.0f, 0.0f, part.local_position,
                part.local_rotation, part.scale, {}, {},
                {feed_scale, feed_scale, feed_scale});
      object.material.emission_strength =
          module.impact_count > 0 && !module.detached ? 0.04f * module.impact_count : 0.0f;
    }
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
                construction_pallet_.pitch, construction_pallet_.roll, part.local_position,
                part.local_rotation, part.scale, {}, {0.0f, 0.0f, 0.0f},
                {1.0f, feed_scale, 1.0f});
    } else {
      placePart(object, construction_pallet_.position, construction_pallet_.yaw,
                construction_pallet_.pitch, construction_pallet_.roll, part.local_position,
                part.local_rotation, part.scale);
    }
  }

  for (const ConstructionVisualPart &part : construction_shredder_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    Vec3 rotation{};
    if (roleContains(part.role, "tooth") || roleContains(part.role, "cutter drum") ||
        roleContains(part.role, "sprocket")) {
      const float spin = construction_shredder_.active
                             ? status_.elapsed_seconds * 12.0f +
                                   static_cast<float>(part.object_index % 7u) * 0.55f
                             : std::sin(status_.elapsed_seconds * 0.8f) * 0.06f;
      rotation.z += spin;
      objects[part.object_index].material.emission_strength =
          construction_shredder_.active ? 0.10f : 0.0f;
    }
    placePart(objects[part.object_index], construction_shredder_.position,
              construction_shredder_.yaw, 0.0f, 0.0f, part.local_position, part.local_rotation,
              part.scale, {}, rotation);
  }

  const Vec3 hook = craneHookPosition(construction_crane_.position, construction_crane_.yaw,
                                      construction_crane_.boom_yaw,
                                      construction_crane_.hook_height,
                                      construction_crane_.hook_reach);
  for (const ConstructionVisualPart &part : construction_crane_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[part.object_index];
    Vec3 extra_rotation{};
    if (roleContains(part.role, "boom")) {
      extra_rotation.y = construction_crane_.boom_yaw;
    }
    placePart(object, construction_crane_.position, construction_crane_.yaw, 0.0f, 0.0f,
              part.local_position, part.local_rotation, part.scale, {}, extra_rotation);
    if (roleContains(part.role, "lifting hook")) {
      object.transform.position = hook;
    } else if (roleContains(part.role, "hanging cable")) {
      const Vec3 cable_top = construction_crane_.position + Vec3{0.0f, 6.02f, 0.0f};
      object.transform.position = (cable_top + hook) * 0.5f;
      object.transform.scale = {part.scale.x, std::max(cable_top.y - hook.y, 0.18f), part.scale.z};
    }
  }

  const float press_fraction =
      construction_press_.required_strokes > 0
          ? static_cast<float>(construction_press_.stroke_count) /
                static_cast<float>(construction_press_.required_strokes)
          : 0.0f;
  const float press_motion = std::clamp(press_fraction * 0.78f +
                                            construction_press_.stroke_animation * 0.22f,
                                        0.0f, 1.0f);
  for (const ConstructionVisualPart &part : construction_press_.parts) {
    if (part.object_index >= objects.size()) {
      continue;
    }
    RenderObject &object = objects[part.object_index];
    if (roleContains(part.role, "loose scrap")) {
      if (construction_press_.pending_load_count <= 0) {
        hideObject(object);
        continue;
      }
      const float fill =
          static_cast<float>(construction_press_.pending_load_count) / 4.0f;
      placePart(object, construction_press_.position, construction_press_.yaw, 0.0f, 0.0f,
                part.local_position, part.local_rotation, part.scale, {}, {},
                {1.0f, std::max(0.20f, fill), fill});
      continue;
    }
    const Vec3 offset = roleContains(part.role, "ram") ? Vec3{0.0f, 0.0f, -press_motion * 0.82f}
                                                        : Vec3{};
    placePart(object, construction_press_.position, construction_press_.yaw, 0.0f, 0.0f,
              part.local_position, part.local_rotation, part.scale, offset);
    if (roleContains(part.role, "button")) {
      object.material.emission_strength = construction_press_.pending_load_count > 0 ? 0.40f : 0.04f;
    }
  }

  for (const ConstructionBale &bale : construction_bales_) {
    for (const ConstructionVisualPart &part : bale.parts) {
      if (part.object_index >= objects.size()) {
        continue;
      }
      RenderObject &object = objects[part.object_index];
      if (!bale.available && !bale.delivered) {
        hideObject(object);
        continue;
      }
      const float fill = std::clamp(static_cast<float>(bale.load_count) / 4.0f, 0.35f, 1.0f);
      placePart(object, bale.position, bale.yaw, 0.0f, 0.0f, part.local_position,
                part.local_rotation, part.scale, {}, {}, {fill, 1.0f, fill});
    }
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
