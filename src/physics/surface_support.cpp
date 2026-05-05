// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/surface_support.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr float kEpsilon = 0.000001f;
constexpr std::size_t kTargetTrianglesPerSupportCell = 8u;
constexpr int kMaxSupportGridAxisCells = 128;

bool projectTriangleHeight(const aster::Vec3 a, const aster::Vec3 b, const aster::Vec3 c,
                           const aster::Vec2 point, float &height) {
  const float v0x = b.x - a.x;
  const float v0z = b.z - a.z;
  const float v1x = c.x - a.x;
  const float v1z = c.z - a.z;
  const float v2x = point.x - a.x;
  const float v2z = point.y - a.z;
  const float denom = v0x * v1z - v1x * v0z;
  if (std::abs(denom) <= kEpsilon) {
    return false;
  }

  const float u = (v2x * v1z - v1x * v2z) / denom;
  const float v = (v0x * v2z - v2x * v0z) / denom;
  const float w = 1.0f - u - v;
  if (u < -0.0005f || v < -0.0005f || w < -0.0005f) {
    return false;
  }

  height = a.y * w + b.y * u + c.y * v;
  return true;
}

aster::TerrainSurfaceSample higherSample(const aster::TerrainSurfaceSample lhs,
                                         const aster::TerrainSurfaceSample rhs) {
  if (!lhs.valid) {
    return rhs;
  }
  if (!rhs.valid) {
    return lhs;
  }
  return rhs.height > lhs.height ? rhs : lhs;
}

bool withinVerticalRange(const aster::SurfaceSupportQuery &query, const float height) {
  if (!std::isfinite(query.reference_y)) {
    return true;
  }
  const float above = height - query.reference_y;
  if (above > std::max(query.max_above, 0.0f)) {
    return false;
  }
  const float below = query.reference_y - height;
  return below <= std::max(query.max_below, 0.0f);
}

bool verticalRangeCanContain(const aster::SurfaceSupportQuery &query, const float min_y,
                             const float max_y) {
  if (!std::isfinite(query.reference_y)) {
    return true;
  }

  const float low = query.reference_y - std::max(query.max_below, 0.0f);
  const float high = query.reference_y + std::max(query.max_above, 0.0f);
  return max_y >= low && min_y <= high;
}

bool containsFootprint(const aster::Vec2 point, const aster::Vec2 min, const aster::Vec2 max) {
  return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

void expandFootprint(aster::Vec2 &min, aster::Vec2 &max, const aster::Vec3 point) {
  min.x = std::min(min.x, point.x);
  min.y = std::min(min.y, point.z);
  max.x = std::max(max.x, point.x);
  max.y = std::max(max.y, point.z);
}

void expandVertical(float &min_y, float &max_y, const aster::Vec3 point) {
  min_y = std::min(min_y, point.y);
  max_y = std::max(max_y, point.y);
}

int gridCoordinate(const float value, const float origin, const float cell_size,
                   const int cell_count) {
  if (cell_size <= kEpsilon || cell_count <= 0) {
    return 0;
  }
  return std::clamp(static_cast<int>((value - origin) / cell_size), 0, cell_count - 1);
}

std::size_t gridCellIndex(const int column, const int row, const int columns) {
  return static_cast<std::size_t>(row * columns + column);
}

} // namespace

namespace aster {

SupportSurfaceSet::PreparedMeshSupportSurface
SupportSurfaceSet::prepareMeshSupportSurface(MeshSupportSurface surface) {
  SupportSurfaceSet::PreparedMeshSupportSurface prepared;
  prepared.mesh = std::move(surface.mesh);
  prepared.transform = surface.transform;
  prepared.min_normal_y = surface.min_normal_y;
  prepared.enabled = surface.enabled;
  prepared.min = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
  prepared.max = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
  prepared.min_y = std::numeric_limits<float>::infinity();
  prepared.max_y = -std::numeric_limits<float>::infinity();

  if (prepared.mesh == nullptr) {
    return prepared;
  }

  const float min_normal_y = std::max(prepared.min_normal_y, 0.0f);
  prepared.triangles.reserve(prepared.mesh->indices.size() / 3u);
  for (std::size_t i = 0; i + 2u < prepared.mesh->indices.size(); i += 3u) {
    const std::uint32_t ia = prepared.mesh->indices[i + 0u];
    const std::uint32_t ib = prepared.mesh->indices[i + 1u];
    const std::uint32_t ic = prepared.mesh->indices[i + 2u];
    if (ia >= prepared.mesh->vertices.size() || ib >= prepared.mesh->vertices.size() ||
        ic >= prepared.mesh->vertices.size()) {
      continue;
    }

    PreparedMeshTriangle triangle;
    triangle.a = transformPoint(prepared.transform, prepared.mesh->vertices[ia].position);
    triangle.b = transformPoint(prepared.transform, prepared.mesh->vertices[ib].position);
    triangle.c = transformPoint(prepared.transform, prepared.mesh->vertices[ic].position);
    triangle.normal = normalize(cross(triangle.b - triangle.a, triangle.c - triangle.a));
    if (triangle.normal.y < min_normal_y) {
      continue;
    }

    triangle.min = {std::min({triangle.a.x, triangle.b.x, triangle.c.x}),
                    std::min({triangle.a.z, triangle.b.z, triangle.c.z})};
    triangle.max = {std::max({triangle.a.x, triangle.b.x, triangle.c.x}),
                    std::max({triangle.a.z, triangle.b.z, triangle.c.z})};
    triangle.min_y = std::min({triangle.a.y, triangle.b.y, triangle.c.y});
    triangle.max_y = std::max({triangle.a.y, triangle.b.y, triangle.c.y});
    prepared.triangles.push_back(triangle);
    expandFootprint(prepared.min, prepared.max, triangle.a);
    expandFootprint(prepared.min, prepared.max, triangle.b);
    expandFootprint(prepared.min, prepared.max, triangle.c);
    expandVertical(prepared.min_y, prepared.max_y, triangle.a);
    expandVertical(prepared.min_y, prepared.max_y, triangle.b);
    expandVertical(prepared.min_y, prepared.max_y, triangle.c);
  }

  if (prepared.triangles.empty()) {
    prepared.min = {};
    prepared.max = {};
    prepared.min_y = 0.0f;
    prepared.max_y = 0.0f;
  } else {
    buildPreparedMeshSupportGrid(prepared);
  }

  return prepared;
}

void SupportSurfaceSet::buildPreparedMeshSupportGrid(PreparedMeshSupportSurface &surface) {
  surface.grid_cells.clear();
  surface.grid_columns = 0;
  surface.grid_rows = 0;
  surface.grid_cell_width = 0.0f;
  surface.grid_cell_depth = 0.0f;

  if (surface.triangles.size() <= kTargetTrianglesPerSupportCell) {
    return;
  }

  const float width = surface.max.x - surface.min.x;
  const float depth = surface.max.y - surface.min.y;
  if (width <= kEpsilon || depth <= kEpsilon) {
    return;
  }

  const float target_cells = std::ceil(static_cast<float>(surface.triangles.size()) /
                                       static_cast<float>(kTargetTrianglesPerSupportCell));
  const float aspect = width / depth;
  const int columns = std::clamp(static_cast<int>(std::ceil(std::sqrt(target_cells * aspect))), 1,
                                 kMaxSupportGridAxisCells);
  const int rows = std::clamp(static_cast<int>(std::ceil(std::sqrt(target_cells / aspect))), 1,
                              kMaxSupportGridAxisCells);
  if (columns * rows <= 1) {
    return;
  }

  surface.grid_min = surface.min;
  surface.grid_columns = columns;
  surface.grid_rows = rows;
  surface.grid_cell_width = width / static_cast<float>(columns);
  surface.grid_cell_depth = depth / static_cast<float>(rows);
  surface.grid_cells.resize(static_cast<std::size_t>(columns * rows));

  for (std::size_t triangle_index = 0; triangle_index < surface.triangles.size();
       ++triangle_index) {
    const PreparedMeshTriangle &triangle = surface.triangles[triangle_index];
    const int min_column =
        gridCoordinate(triangle.min.x, surface.grid_min.x, surface.grid_cell_width, columns);
    const int max_column =
        gridCoordinate(triangle.max.x, surface.grid_min.x, surface.grid_cell_width, columns);
    const int min_row =
        gridCoordinate(triangle.min.y, surface.grid_min.y, surface.grid_cell_depth, rows);
    const int max_row =
        gridCoordinate(triangle.max.y, surface.grid_min.y, surface.grid_cell_depth, rows);
    for (int row = min_row; row <= max_row; ++row) {
      for (int column = min_column; column <= max_column; ++column) {
        surface.grid_cells[gridCellIndex(column, row, columns)].triangles.push_back(
            static_cast<std::uint32_t>(triangle_index));
      }
    }
  }
}

TerrainSurfaceSample SupportSurfaceSet::samplePreparedMeshSupport(
    const SupportSurfaceSet::PreparedMeshSupportSurface &surface,
    const SurfaceSupportQuery &query) {
  if (surface.triangles.empty() ||
      !containsFootprint(query.world_position, surface.min, surface.max) ||
      !verticalRangeCanContain(query, surface.min_y, surface.max_y)) {
    return {};
  }

  TerrainSurfaceSample best;
  const auto consider_triangle = [&](const SupportSurfaceSet::PreparedMeshTriangle &triangle) {
    if (!containsFootprint(query.world_position, triangle.min, triangle.max) ||
        !verticalRangeCanContain(query, triangle.min_y, triangle.max_y)) {
      return;
    }

    float height = 0.0f;
    if (!projectTriangleHeight(triangle.a, triangle.b, triangle.c, query.world_position, height)) {
      return;
    }
    if (!withinVerticalRange(query, height)) {
      return;
    }
    if (!best.valid || height > best.height) {
      best.valid = true;
      best.height = height;
      best.normal = triangle.normal;
    }
  };

  if (!surface.grid_cells.empty()) {
    const int column = gridCoordinate(query.world_position.x, surface.grid_min.x,
                                      surface.grid_cell_width, surface.grid_columns);
    const int row = gridCoordinate(query.world_position.y, surface.grid_min.y,
                                   surface.grid_cell_depth, surface.grid_rows);
    const SupportSurfaceSet::PreparedMeshSupportCell &cell =
        surface.grid_cells[gridCellIndex(column, row, surface.grid_columns)];
    for (const std::uint32_t triangle_index : cell.triangles) {
      if (triangle_index < surface.triangles.size()) {
        consider_triangle(surface.triangles[triangle_index]);
      }
    }
  } else {
    for (const SupportSurfaceSet::PreparedMeshTriangle &triangle : surface.triangles) {
      consider_triangle(triangle);
    }
  }

  return best;
}

void SupportSurfaceSet::clear() {
  terrain_ = nullptr;
  terrain_placement_validator_.clear();
  has_terrain_placement_validator_ = false;
  meshes_.clear();
  boxes_.clear();
}

void SupportSurfaceSet::setTerrain(const TerrainHeightField *terrain) {
  terrain_ = terrain;
}

void SupportSurfaceSet::setTerrainPlacementValidator(PlacementValidator validator) {
  terrain_placement_validator_ = std::move(validator);
  has_terrain_placement_validator_ = true;
}

void SupportSurfaceSet::clearTerrainPlacementValidator() {
  terrain_placement_validator_.clear();
  has_terrain_placement_validator_ = false;
}

void SupportSurfaceSet::addMesh(MeshSupportSurface surface) {
  if (surface.mesh != nullptr) {
    meshes_.push_back(prepareMeshSupportSurface(std::move(surface)));
  }
}

void SupportSurfaceSet::addBox(BoxSupportSurface surface) {
  if (surface.half_extents.x > 0.0f && surface.half_extents.y > 0.0f &&
      surface.half_extents.z > 0.0f) {
    boxes_.push_back(surface);
  }
}

TerrainSurfaceSample SupportSurfaceSet::sample(const Vec2 world_position) const {
  SurfaceSupportQuery query;
  query.world_position = world_position;
  return sample(query);
}

TerrainSurfaceSample SupportSurfaceSet::sample(const SurfaceSupportQuery &query) const {
  TerrainSurfaceSample best;
  if (terrain_ != nullptr) {
    TerrainSurfaceSample terrain_sample = sampleTerrain(*terrain_, query.world_position);
    if (terrain_sample.valid && has_terrain_placement_validator_ &&
        terrain_placement_validator_.rejectsPoint(
            {query.world_position.x, terrain_sample.height, query.world_position.y})) {
      terrain_sample = {};
    }
    best = terrain_sample;
  }
  if (best.valid && !withinVerticalRange(query, best.height)) {
    best = {};
  }
  for (const BoxSupportSurface &box : boxes_) {
    if (!box.enabled) {
      continue;
    }
    best = higherSample(best, sampleBoxSupport(box, query));
  }
  for (const PreparedMeshSupportSurface &surface : meshes_) {
    if (!surface.enabled || surface.mesh == nullptr) {
      continue;
    }
    best = higherSample(best, samplePreparedMeshSupport(surface, query));
  }
  return best;
}

TerrainSurfaceSample sampleBoxSupport(const BoxSupportSurface &box, const Vec2 world_position) {
  return sampleBoxSupport(box, SurfaceSupportQuery{world_position});
}

TerrainSurfaceSample sampleBoxSupport(const BoxSupportSurface &box,
                                      const SurfaceSupportQuery &query) {
  if (box.half_extents.x <= 0.0f || box.half_extents.y <= 0.0f || box.half_extents.z <= 0.0f) {
    return {};
  }
  const bool inside_x = query.world_position.x >= box.center.x - box.half_extents.x &&
                        query.world_position.x <= box.center.x + box.half_extents.x;
  const bool inside_z = query.world_position.y >= box.center.z - box.half_extents.z &&
                        query.world_position.y <= box.center.z + box.half_extents.z;
  if (!inside_x || !inside_z) {
    return {};
  }
  const float height = box.center.y + box.half_extents.y;
  if (!withinVerticalRange(query, height)) {
    return {};
  }
  return {true, height, {0.0f, 1.0f, 0.0f}};
}

TerrainSurfaceSample sampleMeshSupport(const CpuMesh &mesh, const Transform &transform,
                                       const Vec2 world_position, const float min_normal_y) {
  return sampleMeshSupport(mesh, transform, SurfaceSupportQuery{world_position}, min_normal_y);
}

TerrainSurfaceSample sampleMeshSupport(const CpuMesh &mesh, const Transform &transform,
                                       const SurfaceSupportQuery &query, const float min_normal_y) {
  TerrainSurfaceSample best;
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i + 0u];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }

    const Vec3 a = transformPoint(transform, mesh.vertices[ia].position);
    const Vec3 b = transformPoint(transform, mesh.vertices[ib].position);
    const Vec3 c = transformPoint(transform, mesh.vertices[ic].position);
    const Vec3 normal = normalize(cross(b - a, c - a));
    if (normal.y < std::max(min_normal_y, 0.0f)) {
      continue;
    }

    float height = 0.0f;
    if (!projectTriangleHeight(a, b, c, query.world_position, height)) {
      continue;
    }
    if (!withinVerticalRange(query, height)) {
      continue;
    }
    if (!best.valid || height > best.height) {
      best.valid = true;
      best.height = height;
      best.normal = normal;
    }
  }
  return best;
}

} // namespace aster
