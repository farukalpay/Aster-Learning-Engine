// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/terrain_mesh.hpp"
#include "aster/physics/physics_world.hpp"

#include <cstdint>
#include <functional>

namespace aster {

using TerrainSurfaceSampler = std::function<TerrainSurfaceSample(Vec2)>;
using TerrainSurfaceQuerySampler = std::function<TerrainSurfaceSample(Vec3)>;

struct TerrainCharacterSettings {
  CharacterControllerSettings controller{};
  float support_extent_y = 0.5f;
  float support_radius = 0.0f;
  int support_sample_count = 8;
};

struct TerrainContactSettings {
  float support_extent_y = 0.5f;
  float skin_width = 0.012f;
  float max_correction_distance = 0.75f;
  float support_radius = 0.0f;
  int support_sample_count = 8;
};

struct SurfaceStepAssistSettings {
  float support_extent_y = 0.5f;
  float support_radius = 0.0f;
  int support_sample_count = 8;
  float probe_distance = 0.34f;
  float max_step_up = 0.36f;
  float max_step_down = 0.10f;
  float min_horizontal_speed = 0.12f;
  float skin_width = 0.010f;
};

struct ContinuousCharacterCollisionSettings {
  float radius = 0.0f;
  float sweep_radius_scale = 0.96f;
  float skin_width_fraction = 0.035f;
  float minimum_displacement_fraction = 0.05f;
  std::uint32_t collision_mask = 0xffffffffu;
};

[[nodiscard]] CharacterMoveResult
moveCharacterOnTerrain(PhysicsWorld &world, PhysicsBodyHandle handle,
                       const TerrainHeightField &terrain, const CharacterMoveInput &input,
                       const TerrainCharacterSettings &settings, float dt);

[[nodiscard]] CharacterMoveResult moveCharacterOnSurface(
    PhysicsWorld &world, PhysicsBodyHandle handle, const TerrainSurfaceSampler &surface_sampler,
    const CharacterMoveInput &input, const TerrainCharacterSettings &settings, float dt);
[[nodiscard]] CharacterMoveResult
moveCharacterOnSurface(PhysicsWorld &world, PhysicsBodyHandle handle,
                       const TerrainSurfaceQuerySampler &surface_sampler,
                       const CharacterMoveInput &input, const TerrainCharacterSettings &settings,
                       float dt);

[[nodiscard]] CharacterMoveResult solveTerrainContact(PhysicsWorld &world, PhysicsBodyHandle handle,
                                                      const TerrainHeightField &terrain,
                                                      TerrainContactSettings settings = {});

[[nodiscard]] CharacterMoveResult solveSurfaceContact(PhysicsWorld &world, PhysicsBodyHandle handle,
                                                      const TerrainSurfaceSampler &surface_sampler,
                                                      TerrainContactSettings settings = {});
[[nodiscard]] CharacterMoveResult
solveSurfaceContact(PhysicsWorld &world, PhysicsBodyHandle handle,
                    const TerrainSurfaceQuerySampler &surface_sampler,
                    TerrainContactSettings settings = {});

[[nodiscard]] CharacterMoveResult
applySurfaceStepAssist(PhysicsWorld &world, PhysicsBodyHandle handle,
                       const TerrainSurfaceQuerySampler &surface_sampler, Vec3 desired_velocity,
                       SurfaceStepAssistSettings settings = {});

[[nodiscard]] bool
resolveContinuousHorizontalCollision(PhysicsWorld &world, PhysicsBodyHandle handle,
                                     ContinuousCharacterCollisionSettings settings = {});

} // namespace aster
