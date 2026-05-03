// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <vector>

namespace aster {

struct VoxelCoord {
  int x = 0;
  int y = 0;
  int z = 0;

  [[nodiscard]] bool operator==(const VoxelCoord &other) const {
    return x == other.x && y == other.y && z == other.z;
  }

  [[nodiscard]] bool operator<(const VoxelCoord &other) const {
    if (y != other.y) {
      return y < other.y;
    }
    if (z != other.z) {
      return z < other.z;
    }
    return x < other.x;
  }
};

struct VoxelCell {
  VoxelCoord coord{};
  std::uint32_t channel = 0;
  bool solid = true;
  bool collidable = true;
};

struct VoxelCollisionBox {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
};

struct VoxelMeshBatch {
  std::uint32_t channel = 0;
  CpuMesh mesh{};
};

struct VoxelStructure {
  std::vector<VoxelMeshBatch> mesh_batches;
  std::vector<VoxelCollisionBox> collision_boxes;
};

struct VoxelStructureBuildOptions {
  Vec3 cell_size{1.0f, 1.0f, 1.0f};
  Vec3 origin{};
  float uv_scale = 1.0f;
  bool merge_collision_boxes = true;
  bool vertex_ambient_occlusion = true;
  float vertex_ambient_occlusion_min = 0.58f;
};

void appendVoxelBox(std::vector<VoxelCell> &cells, VoxelCoord min, VoxelCoord extent,
                    std::uint32_t channel, bool collidable = true);

[[nodiscard]] VoxelStructure buildVoxelStructure(const std::vector<VoxelCell> &cells,
                                                 VoxelStructureBuildOptions options = {});

} // namespace aster
