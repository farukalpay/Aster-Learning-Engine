// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/mining_system.hpp"

#include <algorithm>

namespace aster {

MiningToolStats starterPickaxeStats() {
  return {.item_id = "pickaxe",
          .tier = 0,
          .power = 1.0f,
          .cooldown_seconds = 0.58f,
          .carve_radius = 0.45f,
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

MiningState::DamageRecord *MiningState::findDamage(const VoxelCellCoord cell) {
  const auto found =
      std::find_if(damage_.begin(), damage_.end(), [cell](const DamageRecord &record) {
        return record.cell == cell;
      });
  return found == damage_.end() ? nullptr : &*found;
}

const MiningState::DamageRecord *MiningState::findDamage(const VoxelCellCoord cell) const {
  const auto found =
      std::find_if(damage_.begin(), damage_.end(), [cell](const DamageRecord &record) {
        return record.cell == cell;
      });
  return found == damage_.end() ? nullptr : &*found;
}

MiningFeedback MiningState::tryMine(const MiningAttempt &attempt) {
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

  DamageRecord *record = findDamage(attempt.hit.cell);
  if (record == nullptr) {
    damage_.push_back({attempt.hit.cell, 0.0f});
    record = &damage_.back();
  }

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
  feedback.edit = {.center = attempt.hit.point - attempt.hit.normal * 0.16f,
                   .radius = std::max(attempt.tool.carve_radius, 0.20f)};
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
  feedback.impact_events.push_back({.kind = VoxelImpactEventKind::Carve,
                                    .point = feedback.edit.center,
                                    .normal = feedback.impact_normal,
                                    .material = feedback.material,
                                    .intensity = 1.0f,
                                    .crack_fraction = 1.0f,
                                    .particle_count = 0});
  damage_.erase(std::remove_if(damage_.begin(), damage_.end(),
                               [&](const DamageRecord &value) {
                                 return value.cell == attempt.hit.cell;
                               }),
                damage_.end());
  return feedback;
}

} // namespace aster
