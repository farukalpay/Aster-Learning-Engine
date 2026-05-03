// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/voxel_structure.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace aster {
namespace {

struct VoxelCoordHash {
  std::size_t operator()(const VoxelCoord &coord) const {
    std::size_t hash = 0;
    const auto mix = [&](const int value) {
      hash ^= std::hash<int>{}(value) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    };
    mix(coord.x);
    mix(coord.y);
    mix(coord.z);
    return hash;
  }
};

struct FaceDef {
  VoxelCoord neighbor{};
  std::array<Vec3, 4> corners{};
};

void validateOptions(const VoxelStructureBuildOptions &options) {
  if (options.cell_size.x <= 0.0f || options.cell_size.y <= 0.0f || options.cell_size.z <= 0.0f) {
    throw std::invalid_argument("Voxel structure cell size must be positive.");
  }
}

std::vector<VoxelCell> normalizedCells(const std::vector<VoxelCell> &cells) {
  std::unordered_set<VoxelCoord, VoxelCoordHash> seen;
  std::vector<VoxelCell> out;
  out.reserve(cells.size());
  for (const VoxelCell &cell : cells) {
    if (!cell.solid) {
      continue;
    }
    if (!seen.insert(cell.coord).second) {
      throw std::invalid_argument("Voxel structure contains duplicate solid cells.");
    }
    out.push_back(cell);
  }
  std::sort(out.begin(), out.end(), [](const VoxelCell &lhs, const VoxelCell &rhs) {
    if (lhs.coord == rhs.coord) {
      return lhs.channel < rhs.channel;
    }
    return lhs.coord < rhs.coord;
  });
  return out;
}

Vec3 localPoint(const VoxelStructureBuildOptions &options, const VoxelCoord coord) {
  return {options.origin.x + static_cast<float>(coord.x) * options.cell_size.x,
          options.origin.y + static_cast<float>(coord.y) * options.cell_size.y,
          options.origin.z + static_cast<float>(coord.z) * options.cell_size.z};
}

Vec2 faceUv(const Vec3 point, const Vec3 normal, const float uv_scale) {
  const float scale = uv_scale <= 0.0001f ? 1.0f : uv_scale;
  const Vec3 abs_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z) {
    return {point.x / scale, point.z / scale};
  }
  if (abs_normal.x >= abs_normal.z) {
    return {point.z / scale, point.y / scale};
  }
  return {point.x / scale, point.y / scale};
}

int axisSign(const float value, const float min, const float max) {
  const float midpoint = (min + max) * 0.5f;
  return value < midpoint ? -1 : 1;
}

bool occupied(const std::unordered_map<VoxelCoord, const VoxelCell *, VoxelCoordHash> &lookup,
              const VoxelCoord coord) {
  return lookup.contains(coord);
}

float voxelCornerAmbientOcclusion(
    const std::unordered_map<VoxelCoord, const VoxelCell *, VoxelCoordHash> &lookup,
    const VoxelCoord cell, const VoxelCoord face_normal, const VoxelCoord corner_sign,
    const float minimum) {
  std::array<VoxelCoord, 2> tangent_axes{};
  int axis_count = 0;
  if (face_normal.x == 0) {
    tangent_axes[axis_count++] = {corner_sign.x, 0, 0};
  }
  if (face_normal.y == 0) {
    tangent_axes[axis_count++] = {0, corner_sign.y, 0};
  }
  if (face_normal.z == 0) {
    tangent_axes[axis_count++] = {0, 0, corner_sign.z};
  }
  if (axis_count != 2) {
    return 1.0f;
  }

  const VoxelCoord side_a{cell.x + face_normal.x + tangent_axes[0].x,
                          cell.y + face_normal.y + tangent_axes[0].y,
                          cell.z + face_normal.z + tangent_axes[0].z};
  const VoxelCoord side_b{cell.x + face_normal.x + tangent_axes[1].x,
                          cell.y + face_normal.y + tangent_axes[1].y,
                          cell.z + face_normal.z + tangent_axes[1].z};
  const VoxelCoord diagonal{cell.x + face_normal.x + tangent_axes[0].x + tangent_axes[1].x,
                            cell.y + face_normal.y + tangent_axes[0].y + tangent_axes[1].y,
                            cell.z + face_normal.z + tangent_axes[0].z + tangent_axes[1].z};
  const int side_a_blocked = occupied(lookup, side_a) ? 1 : 0;
  const int side_b_blocked = occupied(lookup, side_b) ? 1 : 0;
  const int diagonal_blocked = occupied(lookup, diagonal) ? 1 : 0;
  const int open_score = side_a_blocked != 0 && side_b_blocked != 0
                             ? 0
                             : 3 - (side_a_blocked + side_b_blocked + diagonal_blocked);
  const float normalized = static_cast<float>(std::clamp(open_score, 0, 3)) / 3.0f;
  return std::lerp(std::clamp(minimum, 0.0f, 1.0f), 1.0f, normalized);
}

float vertexAmbientOcclusion(
    const std::unordered_map<VoxelCoord, const VoxelCell *, VoxelCoordHash> &lookup,
    const VoxelStructureBuildOptions &options, const VoxelCoord cell, const VoxelCoord face_normal,
    const Vec3 corner, const Vec3 min, const Vec3 max) {
  if (!options.vertex_ambient_occlusion) {
    return 1.0f;
  }
  const VoxelCoord corner_sign{axisSign(corner.x, min.x, max.x), axisSign(corner.y, min.y, max.y),
                               axisSign(corner.z, min.z, max.z)};
  return voxelCornerAmbientOcclusion(lookup, cell, face_normal, corner_sign,
                                     options.vertex_ambient_occlusion_min);
}

void appendQuad(CpuMesh &mesh, const VoxelCoord cell, const FaceDef &face, const Vec3 min,
                const Vec3 max,
                const std::unordered_map<VoxelCoord, const VoxelCell *, VoxelCoordHash> &lookup,
                const VoxelStructureBuildOptions &options) {
  const Vec3 a = face.corners[0];
  const Vec3 b = face.corners[1];
  const Vec3 c = face.corners[2];
  const Vec3 d = face.corners[3];
  const VoxelCoord face_normal{face.neighbor.x - cell.x, face.neighbor.y - cell.y,
                               face.neighbor.z - cell.z};
  const Vec3 normal = normalize(cross(b - a, c - a));
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(
      {a,
       normal,
       faceUv(a, normal, options.uv_scale),
       {1.0f, 0.0f, 0.0f, 1.0f},
       vertexAmbientOcclusion(lookup, options, cell, face_normal, a, min, max)});
  mesh.vertices.push_back(
      {b,
       normal,
       faceUv(b, normal, options.uv_scale),
       {1.0f, 0.0f, 0.0f, 1.0f},
       vertexAmbientOcclusion(lookup, options, cell, face_normal, b, min, max)});
  mesh.vertices.push_back(
      {c,
       normal,
       faceUv(c, normal, options.uv_scale),
       {1.0f, 0.0f, 0.0f, 1.0f},
       vertexAmbientOcclusion(lookup, options, cell, face_normal, c, min, max)});
  mesh.vertices.push_back(
      {d,
       normal,
       faceUv(d, normal, options.uv_scale),
       {1.0f, 0.0f, 0.0f, 1.0f},
       vertexAmbientOcclusion(lookup, options, cell, face_normal, d, min, max)});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
}

std::array<FaceDef, 6> cellFaces(const VoxelCoord coord, const Vec3 min, const Vec3 max) {
  const Vec3 p000{min.x, min.y, min.z};
  const Vec3 p100{max.x, min.y, min.z};
  const Vec3 p010{min.x, max.y, min.z};
  const Vec3 p110{max.x, max.y, min.z};
  const Vec3 p001{min.x, min.y, max.z};
  const Vec3 p101{max.x, min.y, max.z};
  const Vec3 p011{min.x, max.y, max.z};
  const Vec3 p111{max.x, max.y, max.z};

  return {
      FaceDef{{coord.x - 1, coord.y, coord.z}, {p000, p001, p011, p010}},
      FaceDef{{coord.x + 1, coord.y, coord.z}, {p101, p100, p110, p111}},
      FaceDef{{coord.x, coord.y - 1, coord.z}, {p001, p000, p100, p101}},
      FaceDef{{coord.x, coord.y + 1, coord.z}, {p010, p011, p111, p110}},
      FaceDef{{coord.x, coord.y, coord.z - 1}, {p100, p000, p010, p110}},
      FaceDef{{coord.x, coord.y, coord.z + 1}, {p001, p101, p111, p011}},
  };
}

VoxelCollisionBox collisionBoxFor(const VoxelCoord min, const VoxelCoord extent,
                                  const VoxelStructureBuildOptions &options) {
  const Vec3 lower = localPoint(options, min);
  const Vec3 size{static_cast<float>(extent.x) * options.cell_size.x,
                  static_cast<float>(extent.y) * options.cell_size.y,
                  static_cast<float>(extent.z) * options.cell_size.z};
  return {lower + size * 0.5f, size * 0.5f};
}

bool containsAll(const std::unordered_set<VoxelCoord, VoxelCoordHash> &cells, const VoxelCoord min,
                 const VoxelCoord extent) {
  for (int y = 0; y < extent.y; ++y) {
    for (int z = 0; z < extent.z; ++z) {
      for (int x = 0; x < extent.x; ++x) {
        if (!cells.contains({min.x + x, min.y + y, min.z + z})) {
          return false;
        }
      }
    }
  }
  return true;
}

void eraseBox(std::unordered_set<VoxelCoord, VoxelCoordHash> &cells, const VoxelCoord min,
              const VoxelCoord extent) {
  for (int y = 0; y < extent.y; ++y) {
    for (int z = 0; z < extent.z; ++z) {
      for (int x = 0; x < extent.x; ++x) {
        cells.erase({min.x + x, min.y + y, min.z + z});
      }
    }
  }
}

std::vector<VoxelCollisionBox> buildCollisionBoxes(const std::vector<VoxelCell> &cells,
                                                   const VoxelStructureBuildOptions &options) {
  std::unordered_set<VoxelCoord, VoxelCoordHash> remaining;
  for (const VoxelCell &cell : cells) {
    if (cell.collidable) {
      remaining.insert(cell.coord);
    }
  }

  std::vector<VoxelCollisionBox> boxes;
  while (!remaining.empty()) {
    VoxelCoord min = *std::min_element(remaining.begin(), remaining.end());
    VoxelCoord extent{1, 1, 1};
    if (options.merge_collision_boxes) {
      while (containsAll(remaining, min, {extent.x + 1, extent.y, extent.z})) {
        ++extent.x;
      }
      while (containsAll(remaining, min, {extent.x, extent.y, extent.z + 1})) {
        ++extent.z;
      }
      while (containsAll(remaining, min, {extent.x, extent.y + 1, extent.z})) {
        ++extent.y;
      }
    }
    boxes.push_back(collisionBoxFor(min, extent, options));
    eraseBox(remaining, min, extent);
  }
  return boxes;
}

} // namespace

void appendVoxelBox(std::vector<VoxelCell> &cells, const VoxelCoord min, const VoxelCoord extent,
                    const std::uint32_t channel, const bool collidable) {
  if (extent.x <= 0 || extent.y <= 0 || extent.z <= 0) {
    throw std::invalid_argument("Voxel box extent must be positive.");
  }
  for (int y = 0; y < extent.y; ++y) {
    for (int z = 0; z < extent.z; ++z) {
      for (int x = 0; x < extent.x; ++x) {
        cells.push_back({{min.x + x, min.y + y, min.z + z}, channel, true, collidable});
      }
    }
  }
}

VoxelStructure buildVoxelStructure(const std::vector<VoxelCell> &cells,
                                   const VoxelStructureBuildOptions options) {
  validateOptions(options);
  const std::vector<VoxelCell> normalized = normalizedCells(cells);

  std::unordered_map<VoxelCoord, const VoxelCell *, VoxelCoordHash> lookup;
  lookup.reserve(normalized.size());
  for (const VoxelCell &cell : normalized) {
    lookup.emplace(cell.coord, &cell);
  }

  std::unordered_map<std::uint32_t, CpuMesh> meshes;
  for (const VoxelCell &cell : normalized) {
    const Vec3 min = localPoint(options, cell.coord);
    const Vec3 max = min + options.cell_size;
    for (const FaceDef &face : cellFaces(cell.coord, min, max)) {
      if (lookup.contains(face.neighbor)) {
        continue;
      }
      CpuMesh &mesh = meshes[cell.channel];
      appendQuad(mesh, cell.coord, face, min, max, lookup, options);
    }
  }

  VoxelStructure structure;
  structure.mesh_batches.reserve(meshes.size());
  for (auto &[channel, mesh] : meshes) {
    if (!mesh.indices.empty()) {
      structure.mesh_batches.push_back({channel, std::move(mesh)});
    }
  }
  std::sort(structure.mesh_batches.begin(), structure.mesh_batches.end(),
            [](const VoxelMeshBatch &lhs, const VoxelMeshBatch &rhs) {
              return lhs.channel < rhs.channel;
            });
  structure.collision_boxes = buildCollisionBoxes(normalized, options);
  return structure;
}

} // namespace aster
