// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/mining_system.hpp"

#include <algorithm>

namespace aster {
namespace {

MineableHit mineableHitFromVoxel(const VoxelCaveHit &hit) {
  return {.hit = hit.hit,
          .target_key = {},
          .point = hit.point,
          .normal = hit.normal,
          .material = hit.material,
          .cell = hit.cell};
}

} // namespace

MiningToolStats starterPickaxeStats() {
  return {.item_id = "pickaxe",
          .tier = 0,
          .power = 1.0f,
          .cooldown_seconds = 0.58f,
          .carve_radius = 0.84f,
          .reach = 2.65f,
          .swing_seconds = 0.24f};
}

void MiningState::reset() {
  next_allowed_swing_seconds_ = 0.0f;
  damage_.clear();
}

float MiningState::nextAllowedSwingSeconds() const {
  return next_allowed_swing_seconds_;
}

MiningState::DamageRecord *MiningState::findDamage(const MineableHit &hit,
                                                   const float merge_radius) {
  const float radius = std::max(merge_radius, 0.0f);
  const auto found = std::find_if(damage_.begin(), damage_.end(),
                                  [&](const DamageRecord &record) {
                                    if (!hit.target_key.empty() || !record.target_key.empty()) {
                                      return record.target_key == hit.target_key;
                                    }
                                    if (record.cell == hit.cell) {
                                      return true;
                                    }
                                    return record.material == hit.material &&
                                           length(record.point - hit.point) <= radius;
                                  });
  return found == damage_.end() ? nullptr : &*found;
}

const MiningState::DamageRecord *MiningState::findDamage(const MineableHit &hit,
                                                         const float merge_radius) const {
  const float radius = std::max(merge_radius, 0.0f);
  const auto found = std::find_if(damage_.begin(), damage_.end(),
                                  [&](const DamageRecord &record) {
                                    if (!hit.target_key.empty() || !record.target_key.empty()) {
                                      return record.target_key == hit.target_key;
                                    }
                                    if (record.cell == hit.cell) {
                                      return true;
                                    }
                                    return record.material == hit.material &&
                                           length(record.point - hit.point) <= radius;
                                  });
  return found == damage_.end() ? nullptr : &*found;
}

MiningFeedback MiningState::tryMine(const MiningAttempt &attempt) {
  const MineableAttempt generic{.now_seconds = attempt.now_seconds,
                                .hit = mineableHitFromVoxel(attempt.hit),
                                .tool = attempt.tool,
                                .material_hardness = attempt.material_hardness,
                                .resource_item_id = attempt.resource_item_id,
                                .resource_quantity = attempt.resource_quantity,
                                .carve_surface = true};
  return tryMine(generic);
}

MiningFeedback MiningState::tryMine(const MineableAttempt &attempt) {
  MiningFeedback feedback;
  feedback.impact_point = attempt.hit.point;
  feedback.impact_normal = attempt.hit.normal;
  feedback.material = attempt.hit.material;

  if (!attempt.hit.hit || attempt.hit.material == VoxelCaveMaterial::Air ||
      attempt.tool.power <= 0.0f || attempt.material_hardness <= 0.0f) {
    return feedback;
  }
  if (attempt.now_seconds < next_allowed_swing_seconds_) {
    feedback.cooldown_remaining = next_allowed_swing_seconds_ - attempt.now_seconds;
    return feedback;
  }

  feedback.accepted = true;
  next_allowed_swing_seconds_ =
      attempt.now_seconds + std::max(attempt.tool.cooldown_seconds, 0.05f);

  const float merge_radius = std::max(attempt.tool.carve_radius, 0.0f);
  DamageRecord *record = findDamage(attempt.hit, merge_radius);
  if (record == nullptr) {
    damage_.push_back(
        {attempt.hit.target_key, attempt.hit.cell, attempt.hit.point, attempt.hit.material, 0.0f});
    record = &damage_.back();
  }

  record->target_key = attempt.hit.target_key;
  record->cell = attempt.hit.cell;
  record->point = (record->point + attempt.hit.point) * 0.5f;
  record->material = attempt.hit.material;
  record->damage += std::max(attempt.tool.power, 0.0f);
  const float hardness = std::max(attempt.material_hardness, 0.1f);
  feedback.crack_fraction = std::clamp(record->damage / hardness, 0.0f, 1.0f);
  feedback.impact_events.push_back({.kind = VoxelImpactEventKind::SurfaceHit,
                                    .point = feedback.impact_point,
                                    .normal = feedback.impact_normal,
                                    .material = feedback.material,
                                    .intensity = std::max(attempt.tool.power / hardness, 0.05f),
                                    .crack_fraction = feedback.crack_fraction,
                                    .particle_count = 2});
  feedback.impact_events.push_back({.kind = VoxelImpactEventKind::Crack,
                                    .point = feedback.impact_point,
                                    .normal = feedback.impact_normal,
                                    .material = feedback.material,
                                    .intensity = feedback.crack_fraction,
                                    .crack_fraction = feedback.crack_fraction,
                                    .particle_count = 0});
  if (record->damage < hardness) {
    return feedback;
  }

  feedback.carved = true;
  const float carve_radius = std::max(attempt.tool.carve_radius, 0.20f);
  if (attempt.carve_surface) {
    feedback.edit = {.center = attempt.hit.point -
                               attempt.hit.normal * std::max(carve_radius * 0.34f, 0.16f),
                     .radius = carve_radius};
  }
  feedback.resource_item_id = attempt.resource_item_id;
  feedback.resource_quantity = std::max(attempt.resource_quantity, 0);
  feedback.impact_events.push_back({.kind = VoxelImpactEventKind::ShardBurst,
                                    .point = feedback.impact_point,
                                    .normal = feedback.impact_normal,
                                    .material = feedback.material,
                                    .intensity = std::max(attempt.tool.power / hardness, 0.10f),
                                    .crack_fraction = 1.0f,
                                    .particle_count = 9});
  feedback.impact_events.push_back({.kind = VoxelImpactEventKind::DustBurst,
                                    .point = feedback.impact_point,
                                    .normal = feedback.impact_normal,
                                    .material = feedback.material,
                                    .intensity = 0.65f,
                                    .crack_fraction = 1.0f,
                                    .particle_count = 7});
  if (attempt.carve_surface) {
    feedback.impact_events.push_back({.kind = VoxelImpactEventKind::Carve,
                                      .point = feedback.edit.center,
                                      .normal = feedback.impact_normal,
                                      .material = feedback.material,
                                      .intensity = 1.0f,
                                      .crack_fraction = 1.0f,
                                      .particle_count = 0});
  }
  const std::string carved_key = record->target_key;
  const Vec3 carved_point = record->point;
  const VoxelCellCoord carved_cell = record->cell;
  const VoxelCaveMaterial carved_material = record->material;
  damage_.erase(std::remove_if(damage_.begin(), damage_.end(),
                               [&](const DamageRecord &value) {
                                 if (!carved_key.empty() || !value.target_key.empty()) {
                                   return value.target_key == carved_key;
                                 }
                                 return value.cell == carved_cell ||
                                        (value.material == carved_material &&
                                         length(value.point - carved_point) <= merge_radius);
                               }),
                damage_.end());
  return feedback;
}

} // namespace aster
