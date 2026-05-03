// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/terrain_mesh.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct TerrainSplinePath {
  int samples = 64;
  Vec3 start{};
  Vec3 control{};
  Vec3 control_b{};
  Vec3 end{};
};

struct TerrainSplineRoute {
  std::vector<TerrainSplinePath> segments{};
};

struct TerrainMountainBrushSpec {
  std::uint32_t seed = 1u;
  Vec2 center{};
  Vec2 radius{8.0f, 8.0f};
  float height = 3.0f;
  float shoulder_height = 0.6f;
  float surface_noise = 0.24f;
  float ridge_frequency = 0.12f;
  float inner_plateau = 0.18f;
};

struct TerrainPathDeformationSpec {
  TerrainSplinePath path{};
  float half_width = 0.44f;
  float shoulder_width = 0.85f;
  float crown_height = 0.025f;
  float surface_noise = 0.0f;
  float max_raise = 100000.0f;
  float max_lower = 100000.0f;
  std::uint32_t seed = 1u;
};

struct TerrainRouteDeformationSpec {
  TerrainSplineRoute route{};
  float half_width = 0.44f;
  float shoulder_width = 0.85f;
  float crown_height = 0.025f;
  float surface_noise = 0.0f;
  float max_raise = 100000.0f;
  float max_lower = 100000.0f;
  std::uint32_t seed = 1u;
};

struct TerrainPortalShelfSpec {
  std::uint32_t seed = 1u;
  Vec3 entrance{};
  Vec3 inward{0.0f, 0.0f, -1.0f};
  float floor_height = 0.0f;
  float half_width = 1.45f;
  float front_depth = 1.45f;
  float back_depth = 2.70f;
  float feather = 1.10f;
  float side_berm_height = 0.72f;
  float rear_cover_height = 1.10f;
  float surface_noise = 0.12f;
};

[[nodiscard]] Vec3 evaluateTerrainSplinePath(const TerrainSplinePath &path, float t);
[[nodiscard]] Vec3 evaluateTerrainSplineTangent(const TerrainSplinePath &path, float t);

void applyTerrainMountainBrush(TerrainHeightField &terrain, const TerrainMountainBrushSpec &spec);
void deformTerrainAlongPath(TerrainHeightField &terrain, const TerrainPathDeformationSpec &spec);
void deformTerrainAlongRoute(TerrainHeightField &terrain, const TerrainRouteDeformationSpec &spec);
void sculptTerrainPortalShelf(TerrainHeightField &terrain, const TerrainPortalShelfSpec &spec);

} // namespace aster
