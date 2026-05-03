// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/geometry/terrain_mesh.hpp"
#include "aster/math/transform.hpp"
#include "aster/physics/placement_validation.hpp"
#include "aster/render/mesh.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace aster {

struct MeshSupportSurface {
  std::shared_ptr<const CpuMesh> mesh{};
  Transform transform{};
  float min_normal_y = 0.15f;
  bool enabled = true;
};

struct BoxSupportSurface {
  Vec3 center{};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
  bool enabled = true;
};

struct SurfaceSupportQuery {
  Vec2 world_position{};
  float reference_y = std::numeric_limits<float>::infinity();
  float max_above = std::numeric_limits<float>::infinity();
  float max_below = std::numeric_limits<float>::infinity();
};

class SupportSurfaceSet {
public:
  void clear();
  void setTerrain(const TerrainHeightField *terrain);
  void setTerrainPlacementValidator(PlacementValidator validator);
  void clearTerrainPlacementValidator();
  void addMesh(MeshSupportSurface surface);
  void addBox(BoxSupportSurface surface);

  [[nodiscard]] TerrainSurfaceSample sample(Vec2 world_position) const;
  [[nodiscard]] TerrainSurfaceSample sample(const SurfaceSupportQuery &query) const;

private:
  struct PreparedMeshTriangle {
    Vec3 a{};
    Vec3 b{};
    Vec3 c{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec2 min{};
    Vec2 max{};
    float min_y = 0.0f;
    float max_y = 0.0f;
  };

  struct PreparedMeshSupportCell {
    std::vector<std::uint32_t> triangles;
  };

  struct PreparedMeshSupportSurface {
    std::shared_ptr<const CpuMesh> mesh{};
    Transform transform{};
    float min_normal_y = 0.15f;
    bool enabled = true;
    Vec2 min{};
    Vec2 max{};
    float min_y = 0.0f;
    float max_y = 0.0f;
    std::vector<PreparedMeshTriangle> triangles;
    Vec2 grid_min{};
    float grid_cell_width = 0.0f;
    float grid_cell_depth = 0.0f;
    int grid_columns = 0;
    int grid_rows = 0;
    std::vector<PreparedMeshSupportCell> grid_cells;
  };

  [[nodiscard]] static PreparedMeshSupportSurface
  prepareMeshSupportSurface(MeshSupportSurface surface);
  static void buildPreparedMeshSupportGrid(PreparedMeshSupportSurface &surface);
  [[nodiscard]] static TerrainSurfaceSample
  samplePreparedMeshSupport(const PreparedMeshSupportSurface &surface,
                            const SurfaceSupportQuery &query);

  const TerrainHeightField *terrain_ = nullptr;
  PlacementValidator terrain_placement_validator_{};
  bool has_terrain_placement_validator_ = false;
  std::vector<PreparedMeshSupportSurface> meshes_;
  std::vector<BoxSupportSurface> boxes_;
};

[[nodiscard]] TerrainSurfaceSample sampleMeshSupport(const CpuMesh &mesh,
                                                     const Transform &transform,
                                                     Vec2 world_position,
                                                     float min_normal_y = 0.15f);
[[nodiscard]] TerrainSurfaceSample sampleMeshSupport(const CpuMesh &mesh,
                                                     const Transform &transform,
                                                     const SurfaceSupportQuery &query,
                                                     float min_normal_y = 0.15f);
[[nodiscard]] TerrainSurfaceSample sampleBoxSupport(const BoxSupportSurface &box,
                                                    Vec2 world_position);
[[nodiscard]] TerrainSurfaceSample sampleBoxSupport(const BoxSupportSurface &box,
                                                    const SurfaceSupportQuery &query);

} // namespace aster
