// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <vector>

namespace aster {

struct TerrainHeightField {
  int grid_size = 0;
  float square_size = 1.0f;
  Vec2 origin{};
  std::vector<float> heights;
};

struct TerrainMeshClipEllipse {
  Vec2 center{};
  Vec2 radius{1.0f, 1.0f};
  float radius_x_negative = 0.0f;
  float radius_x_positive = 0.0f;
  float radius_z_negative = 0.0f;
  float radius_z_positive = 0.0f;
};

struct TerrainMeshClipBox {
  Vec2 min{};
  Vec2 max{};
};

struct TerrainMeshClipOrientedEllipse {
  Vec2 center{};
  Vec2 forward{0.0f, 1.0f};
  Vec2 radius{1.0f, 1.0f};
  float radius_side_negative = 0.0f;
  float radius_side_positive = 0.0f;
  float radius_forward_negative = 0.0f;
  float radius_forward_positive = 0.0f;
};

struct TerrainMeshBuildOptions {
  bool alternating_diagonal_split = true;
  bool clamp_edge_samples = true;
  int subdivisions_per_square = 1;
  bool smooth_visual_surface = false;
  std::vector<TerrainMeshClipEllipse> clip_ellipses;
  std::vector<TerrainMeshClipOrientedEllipse> clip_oriented_ellipses;
  std::vector<TerrainMeshClipBox> clip_boxes;
};

struct TerrainSurfaceSample {
  bool valid = false;
  float height = 0.0f;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  int cell_x = 0;
  int cell_z = 0;
};

struct ProceduralTerrainSpec {
  int grid_size = 145;
  float square_size = 0.70f;
  float central_flat_radius = 7.4f;
  float transition_width = 7.5f;
  float hill_height = 1.15f;
  float mountain_height = 2.65f;
};

[[nodiscard]] TerrainHeightField makeProceduralTerrain(const ProceduralTerrainSpec &spec);
[[nodiscard]] TerrainSurfaceSample sampleTerrain(const TerrainHeightField &terrain,
                                                 Vec2 world_position);
[[nodiscard]] Vec3 terrainNormalAt(const TerrainHeightField &terrain, int x, int z);
[[nodiscard]] CpuMesh makeTerrainMesh(const TerrainHeightField &terrain,
                                      TerrainMeshBuildOptions options = {});

} // namespace aster
