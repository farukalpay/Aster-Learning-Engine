// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/voxel_cave.hpp"

#include <string>
#include <vector>

namespace aster {

struct MiningToolStats {
  std::string item_id;
  int tier = 0;
  float power = 1.0f;
  float cooldown_seconds = 0.58f;
  float carve_radius = 0.58f;
  float reach = 2.65f;
  float swing_seconds = 0.22f;
};

struct MiningAttempt {
  float now_seconds = 0.0f;
  VoxelCaveHit hit{};
  MiningToolStats tool{};
  float material_hardness = 0.0f;
  std::string resource_item_id;
  int resource_quantity = 0;
};

struct MineableHit {
  bool hit = false;
  std::string target_key;
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  VoxelCaveMaterial material = VoxelCaveMaterial::Air;
  VoxelCellCoord cell{};
};

struct MineableAttempt {
  float now_seconds = 0.0f;
  MineableHit hit{};
  MiningToolStats tool{};
  float material_hardness = 0.0f;
  std::string resource_item_id;
  int resource_quantity = 0;
  bool carve_surface = true;
};

enum class VoxelImpactEventKind {
  SurfaceHit,
  Crack,
  ShardBurst,
  DustBurst,
  Carve,
};

struct VoxelImpactEvent {
  VoxelImpactEventKind kind = VoxelImpactEventKind::SurfaceHit;
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  VoxelCaveMaterial material = VoxelCaveMaterial::Air;
  float intensity = 0.0f;
  float crack_fraction = 0.0f;
  int particle_count = 0;
};

struct MiningFeedback {
  bool accepted = false;
  bool carved = false;
  float cooldown_remaining = 0.0f;
  float crack_fraction = 0.0f;
  Vec3 impact_point{};
  Vec3 impact_normal{0.0f, 1.0f, 0.0f};
  VoxelEdit edit{};
  VoxelCaveMaterial material = VoxelCaveMaterial::Air;
  std::string resource_item_id;
  int resource_quantity = 0;
  std::vector<VoxelImpactEvent> impact_events;
};

class MiningState {
public:
  void reset();
  [[nodiscard]] MiningFeedback tryMine(const MiningAttempt &attempt);
  [[nodiscard]] MiningFeedback tryMine(const MineableAttempt &attempt);
  [[nodiscard]] float nextAllowedSwingSeconds() const;

private:
  struct DamageRecord {
    std::string target_key;
    VoxelCellCoord cell{};
    Vec3 point{};
    VoxelCaveMaterial material = VoxelCaveMaterial::Air;
    float damage = 0.0f;
  };

  [[nodiscard]] DamageRecord *findDamage(const MineableHit &hit, float merge_radius);
  [[nodiscard]] const DamageRecord *findDamage(const MineableHit &hit, float merge_radius) const;

  float next_allowed_swing_seconds_ = 0.0f;
  std::vector<DamageRecord> damage_;
};

[[nodiscard]] MiningToolStats starterPickaxeStats();

} // namespace aster
