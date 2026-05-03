// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/terrain_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr float kEpsilon = 0.000001f;

float smoothstep(const float edge0, const float edge1, const float value) {
  const float t = aster::clamp((value - edge0) / std::max(edge1 - edge0, kEpsilon), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float hash2(const float x, const float z) {
  return std::sin(x * 127.1f + z * 311.7f) * 43758.5453123f -
         std::floor(std::sin(x * 127.1f + z * 311.7f) * 43758.5453123f);
}

float valueNoise(float x, float z) {
  const float ix = std::floor(x);
  const float iz = std::floor(z);
  const float fx = x - ix;
  const float fz = z - iz;
  const float ux = fx * fx * (3.0f - 2.0f * fx);
  const float uz = fz * fz * (3.0f - 2.0f * fz);

  const float a = hash2(ix, iz);
  const float b = hash2(ix + 1.0f, iz);
  const float c = hash2(ix, iz + 1.0f);
  const float d = hash2(ix + 1.0f, iz + 1.0f);
  return (a * (1.0f - ux) + b * ux) * (1.0f - uz) + (c * (1.0f - ux) + d * ux) * uz;
}

float fbm(float x, float z) {
  float sum = 0.0f;
  float amplitude = 0.55f;
  for (int i = 0; i < 5; ++i) {
    sum += valueNoise(x, z) * amplitude;
    x *= 2.03f;
    z *= 2.03f;
    amplitude *= 0.5f;
  }
  return sum;
}

float ridged(float x, float z) {
  return 1.0f - std::abs(fbm(x, z) * 2.0f - 1.0f);
}

int heightIndex(const aster::TerrainHeightField &terrain, const int x, const int z) {
  return z * terrain.grid_size + x;
}

float heightAtGrid(const aster::TerrainHeightField &terrain, int x, int z) {
  x = std::clamp(x, 0, terrain.grid_size - 1);
  z = std::clamp(z, 0, terrain.grid_size - 1);
  return terrain.heights[heightIndex(terrain, x, z)];
}

bool split45(const int x, const int z) {
  return ((x ^ z) & 1) == 0;
}

void appendTriangle(aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b,
                    const std::uint32_t c) {
  mesh.indices.push_back(a);
  mesh.indices.push_back(b);
  mesh.indices.push_back(c);
}

float directionalRadius(const float fallback, const float negative_radius,
                        const float positive_radius, const float offset) {
  if (offset < 0.0f && negative_radius > 0.0f) {
    return negative_radius;
  }
  if (offset > 0.0f && positive_radius > 0.0f) {
    return positive_radius;
  }
  return fallback;
}

bool insideClipEllipse(const aster::Vec3 position,
                       const aster::TerrainMeshClipEllipse &clip_ellipse) {
  if (clip_ellipse.radius.x <= 0.0f || clip_ellipse.radius.y <= 0.0f) {
    return false;
  }
  const float offset_x = position.x - clip_ellipse.center.x;
  const float offset_z = position.z - clip_ellipse.center.y;
  const float radius_x = directionalRadius(clip_ellipse.radius.x, clip_ellipse.radius_x_negative,
                                           clip_ellipse.radius_x_positive, offset_x);
  const float radius_z = directionalRadius(clip_ellipse.radius.y, clip_ellipse.radius_z_negative,
                                           clip_ellipse.radius_z_positive, offset_z);
  const float x = offset_x / std::max(radius_x, kEpsilon);
  const float z = offset_z / std::max(radius_z, kEpsilon);
  return x * x + z * z < 1.0f;
}

bool insideClipOrientedEllipse(const aster::Vec3 position,
                               const aster::TerrainMeshClipOrientedEllipse &clip_ellipse) {
  if (clip_ellipse.radius.x <= 0.0f || clip_ellipse.radius.y <= 0.0f) {
    return false;
  }
  aster::Vec2 forward = aster::normalize(clip_ellipse.forward);
  if (aster::length(forward) <= 0.0001f) {
    forward = {0.0f, 1.0f};
  }
  const aster::Vec2 side{-forward.y, forward.x};
  const aster::Vec2 offset{position.x - clip_ellipse.center.x, position.z - clip_ellipse.center.y};
  const float side_offset = aster::dot(offset, side);
  const float forward_offset = aster::dot(offset, forward);
  const float radius_side =
      directionalRadius(clip_ellipse.radius.x, clip_ellipse.radius_side_negative,
                        clip_ellipse.radius_side_positive, side_offset);
  const float radius_forward =
      directionalRadius(clip_ellipse.radius.y, clip_ellipse.radius_forward_negative,
                        clip_ellipse.radius_forward_positive, forward_offset);
  const float x = side_offset / std::max(radius_side, kEpsilon);
  const float z = forward_offset / std::max(radius_forward, kEpsilon);
  return x * x + z * z < 1.0f;
}

bool validClipBox(const aster::TerrainMeshClipBox &clip_box) {
  return clip_box.min.x <= clip_box.max.x && clip_box.min.y <= clip_box.max.y;
}

bool triangleOverlapsClipBox(const aster::CpuMesh &mesh, const std::uint32_t a,
                             const std::uint32_t b, const std::uint32_t c,
                             const aster::TerrainMeshClipBox &clip_box) {
  if (!validClipBox(clip_box)) {
    return false;
  }
  const aster::Vec3 pa = mesh.vertices[a].position;
  const aster::Vec3 pb = mesh.vertices[b].position;
  const aster::Vec3 pc = mesh.vertices[c].position;
  const aster::Vec2 triangle_min{std::min(pa.x, std::min(pb.x, pc.x)),
                                 std::min(pa.z, std::min(pb.z, pc.z))};
  const aster::Vec2 triangle_max{std::max(pa.x, std::max(pb.x, pc.x)),
                                 std::max(pa.z, std::max(pb.z, pc.z))};
  return triangle_min.x <= clip_box.max.x && triangle_max.x >= clip_box.min.x &&
         triangle_min.y <= clip_box.max.y && triangle_max.y >= clip_box.min.y;
}

bool triangleClipped(
    const aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b, const std::uint32_t c,
    const std::vector<aster::TerrainMeshClipEllipse> &clip_ellipses,
    const std::vector<aster::TerrainMeshClipOrientedEllipse> &clip_oriented_ellipses,
    const std::vector<aster::TerrainMeshClipBox> &clip_boxes) {
  if (clip_ellipses.empty() && clip_oriented_ellipses.empty() && clip_boxes.empty()) {
    return false;
  }
  const aster::Vec3 centroid =
      (mesh.vertices[a].position + mesh.vertices[b].position + mesh.vertices[c].position) / 3.0f;
  for (const aster::TerrainMeshClipEllipse &clip_ellipse : clip_ellipses) {
    if (insideClipEllipse(centroid, clip_ellipse)) {
      return true;
    }
  }
  for (const aster::TerrainMeshClipOrientedEllipse &clip_ellipse : clip_oriented_ellipses) {
    if (insideClipOrientedEllipse(centroid, clip_ellipse)) {
      return true;
    }
  }
  for (const aster::TerrainMeshClipBox &clip_box : clip_boxes) {
    if (triangleOverlapsClipBox(mesh, a, b, c, clip_box)) {
      return true;
    }
  }
  return false;
}

void appendTerrainTriangle(
    aster::CpuMesh &mesh, const std::uint32_t a, const std::uint32_t b, const std::uint32_t c,
    const std::vector<aster::TerrainMeshClipEllipse> &clip_ellipses,
    const std::vector<aster::TerrainMeshClipOrientedEllipse> &clip_oriented_ellipses,
    const std::vector<aster::TerrainMeshClipBox> &clip_boxes) {
  if (!triangleClipped(mesh, a, b, c, clip_ellipses, clip_oriented_ellipses, clip_boxes)) {
    appendTriangle(mesh, a, b, c);
  }
}

float catmullRom(const float p0, const float p1, const float p2, const float p3, const float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float smoothHeightAtGridCoordinate(const aster::TerrainHeightField &terrain, const float grid_x,
                                   const float grid_z) {
  const int base_x = static_cast<int>(std::floor(grid_x));
  const int base_z = static_cast<int>(std::floor(grid_z));
  const float tx = grid_x - static_cast<float>(base_x);
  const float tz = grid_z - static_cast<float>(base_z);

  float rows[4]{};
  float min_height = std::numeric_limits<float>::max();
  float max_height = std::numeric_limits<float>::lowest();
  for (int row = 0; row < 4; ++row) {
    const int sample_z = base_z + row - 1;
    float samples[4]{};
    for (int column = 0; column < 4; ++column) {
      samples[column] = heightAtGrid(terrain, base_x + column - 1, sample_z);
      min_height = std::min(min_height, samples[column]);
      max_height = std::max(max_height, samples[column]);
    }
    rows[row] = catmullRom(samples[0], samples[1], samples[2], samples[3], tx);
  }

  return std::clamp(catmullRom(rows[0], rows[1], rows[2], rows[3], tz), min_height, max_height);
}

float visualHeightAtGridCoordinate(const aster::TerrainHeightField &terrain, const float grid_x,
                                   const float grid_z, const bool smooth_visual_surface) {
  const float x = std::clamp(grid_x, 0.0f, static_cast<float>(terrain.grid_size - 1));
  const float z = std::clamp(grid_z, 0.0f, static_cast<float>(terrain.grid_size - 1));
  if (smooth_visual_surface) {
    return smoothHeightAtGridCoordinate(terrain, x, z);
  }
  const aster::Vec2 world_position{terrain.origin.x + x * terrain.square_size,
                                   terrain.origin.y + z * terrain.square_size};
  const aster::TerrainSurfaceSample sample = aster::sampleTerrain(terrain, world_position);
  return sample.valid ? sample.height : 0.0f;
}

aster::Vec3 visualNormalAtGridCoordinate(const aster::TerrainHeightField &terrain,
                                         const float grid_x, const float grid_z,
                                         const bool smooth_visual_surface, const float grid_step) {
  const float left =
      visualHeightAtGridCoordinate(terrain, grid_x - grid_step, grid_z, smooth_visual_surface);
  const float right =
      visualHeightAtGridCoordinate(terrain, grid_x + grid_step, grid_z, smooth_visual_surface);
  const float down =
      visualHeightAtGridCoordinate(terrain, grid_x, grid_z - grid_step, smooth_visual_surface);
  const float up =
      visualHeightAtGridCoordinate(terrain, grid_x, grid_z + grid_step, smooth_visual_surface);
  return aster::normalize({left - right, terrain.square_size * grid_step * 2.0f, down - up});
}

} // namespace

namespace aster {

TerrainHeightField makeProceduralTerrain(const ProceduralTerrainSpec &spec) {
  if (spec.grid_size < 3 || spec.square_size <= 0.0f) {
    throw std::invalid_argument("Procedural terrain requires grid_size >= 3 and square_size > 0.");
  }

  TerrainHeightField terrain;
  terrain.grid_size = spec.grid_size;
  terrain.square_size = spec.square_size;
  const float extent = static_cast<float>(spec.grid_size - 1) * spec.square_size;
  terrain.origin = {-extent * 0.5f, -extent * 0.5f};
  terrain.heights.resize(static_cast<std::size_t>(spec.grid_size * spec.grid_size));

  for (int z = 0; z < spec.grid_size; ++z) {
    for (int x = 0; x < spec.grid_size; ++x) {
      const float world_x = terrain.origin.x + static_cast<float>(x) * spec.square_size;
      const float world_z = terrain.origin.y + static_cast<float>(z) * spec.square_size;
      const float distance = length(Vec2{world_x, world_z});
      const float terrain_weight = smoothstep(
          spec.central_flat_radius, spec.central_flat_radius + spec.transition_width, distance);
      const float mountain_weight = smoothstep(
          spec.central_flat_radius + spec.transition_width * 1.2f, extent * 0.46f, distance);
      const float broad = fbm(world_x * 0.055f + 10.3f, world_z * 0.055f - 4.7f);
      const float medium = fbm(world_x * 0.13f - 2.1f, world_z * 0.13f + 8.4f);
      const float ridge = ridged(world_x * 0.070f + 4.0f, world_z * 0.070f - 6.0f);
      const float sediment = fbm(world_x * 0.22f + 18.0f, world_z * 0.22f - 12.0f);
      const float gravel = fbm(world_x * 0.46f - 7.0f, world_z * 0.46f + 3.0f);
      const float erosion_mask =
          smoothstep(0.42f, 0.84f, ridge) * smoothstep(0.18f, 0.72f, mountain_weight);
      const float radial_falloff = 1.0f - smoothstep(extent * 0.45f, extent * 0.53f, distance);
      const float hills = (broad * 0.55f + medium * 0.45f - 0.36f) * spec.hill_height;
      const float mountains = ridge * ridge * spec.mountain_height * mountain_weight;
      const float eroded_channels =
          (sediment - broad) * erosion_mask * spec.mountain_height * 0.08f;
      const float surface_breakup =
          ((gravel - 0.5f) * 0.04f + (sediment - 0.5f) * 0.03f) * spec.hill_height;
      const float edge_lift = smoothstep(extent * 0.34f, extent * 0.50f, distance) * 0.42f;
      const float height = (hills + mountains + eroded_channels + surface_breakup + edge_lift) *
                           terrain_weight * radial_falloff;
      terrain.heights[heightIndex(terrain, x, z)] = std::max(height, -0.055f * terrain_weight);
    }
  }

  return terrain;
}

TerrainSurfaceSample sampleTerrain(const TerrainHeightField &terrain, const Vec2 world_position) {
  if (terrain.grid_size < 2 ||
      terrain.heights.size() != static_cast<std::size_t>(terrain.grid_size * terrain.grid_size)) {
    return {};
  }

  const float local_x = (world_position.x - terrain.origin.x) / terrain.square_size;
  const float local_z = (world_position.y - terrain.origin.y) / terrain.square_size;
  if (local_x < 0.0f || local_z < 0.0f || local_x > static_cast<float>(terrain.grid_size - 1) ||
      local_z > static_cast<float>(terrain.grid_size - 1)) {
    return {};
  }

  const int x = std::clamp(static_cast<int>(std::floor(local_x)), 0, terrain.grid_size - 2);
  const int z = std::clamp(static_cast<int>(std::floor(local_z)), 0, terrain.grid_size - 2);
  const float xp = local_x - static_cast<float>(x);
  const float zp = local_z - static_cast<float>(z);

  const float bottom_left = heightAtGrid(terrain, x, z);
  const float bottom_right = heightAtGrid(terrain, x + 1, z);
  const float top_left = heightAtGrid(terrain, x, z + 1);
  const float top_right = heightAtGrid(terrain, x + 1, z + 1);

  TerrainSurfaceSample sample;
  sample.valid = true;
  sample.cell_x = x;
  sample.cell_z = z;
  if (split45(x, z)) {
    if (xp > zp) {
      sample.height =
          bottom_left + xp * (bottom_right - bottom_left) + zp * (top_right - bottom_right);
      sample.normal =
          normalize({bottom_left - bottom_right, terrain.square_size, bottom_right - top_right});
    } else {
      sample.height = bottom_left + xp * (top_right - top_left) + zp * (top_left - bottom_left);
      sample.normal =
          normalize({top_left - top_right, terrain.square_size, bottom_left - top_left});
    }
  } else {
    if (1.0f - xp > zp) {
      sample.height =
          bottom_right + (1.0f - xp) * (bottom_left - bottom_right) + zp * (top_left - bottom_left);
      sample.normal =
          normalize({bottom_left - bottom_right, terrain.square_size, bottom_left - top_left});
    } else {
      sample.height =
          bottom_right + (1.0f - xp) * (top_left - top_right) + zp * (top_right - bottom_right);
      sample.normal =
          normalize({top_left - top_right, terrain.square_size, bottom_right - top_right});
    }
  }
  return sample;
}

Vec3 terrainNormalAt(const TerrainHeightField &terrain, const int x, const int z) {
  if (terrain.grid_size < 2) {
    return {0.0f, 1.0f, 0.0f};
  }
  const float h1 = heightAtGrid(terrain, x + 1, z);
  const float h2 = heightAtGrid(terrain, x, z + 1);
  const float h3 = heightAtGrid(terrain, x - 1, z);
  const float h4 = heightAtGrid(terrain, x, z - 1);
  return normalize({h3 - h1, terrain.square_size * 2.0f, h4 - h2});
}

CpuMesh makeTerrainMesh(const TerrainHeightField &terrain, const TerrainMeshBuildOptions options) {
  if (terrain.grid_size < 2 ||
      terrain.heights.size() != static_cast<std::size_t>(terrain.grid_size * terrain.grid_size)) {
    throw std::invalid_argument("Terrain mesh requires a complete height field.");
  }

  const int subdivisions = std::max(1, options.subdivisions_per_square);
  const int mesh_grid_size = (terrain.grid_size - 1) * subdivisions + 1;
  const int mesh_cell_count = mesh_grid_size - 1;
  const float grid_step = 1.0f / static_cast<float>(subdivisions);

  CpuMesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(mesh_grid_size * mesh_grid_size));
  mesh.indices.reserve(static_cast<std::size_t>(mesh_cell_count * mesh_cell_count * 6));
  const float uv_scale = 1.0f / static_cast<float>(terrain.grid_size - 1);
  for (int z = 0; z < mesh_grid_size; ++z) {
    for (int x = 0; x < mesh_grid_size; ++x) {
      const float grid_x =
          std::min(static_cast<float>(x) * grid_step, static_cast<float>(terrain.grid_size - 1));
      const float grid_z =
          std::min(static_cast<float>(z) * grid_step, static_cast<float>(terrain.grid_size - 1));
      const float world_x = terrain.origin.x + grid_x * terrain.square_size;
      const float world_z = terrain.origin.y + grid_z * terrain.square_size;
      mesh.vertices.push_back(
          {{world_x,
            visualHeightAtGridCoordinate(terrain, grid_x, grid_z, options.smooth_visual_surface),
            world_z},
           visualNormalAtGridCoordinate(terrain, grid_x, grid_z, options.smooth_visual_surface,
                                        grid_step),
           {grid_x * uv_scale, grid_z * uv_scale}});
    }
  }

  for (int z = 0; z < mesh_cell_count; ++z) {
    for (int x = 0; x < mesh_cell_count; ++x) {
      const std::uint32_t bl = static_cast<std::uint32_t>(z * mesh_grid_size + x);
      const std::uint32_t br = static_cast<std::uint32_t>(z * mesh_grid_size + x + 1);
      const std::uint32_t tl = static_cast<std::uint32_t>((z + 1) * mesh_grid_size + x);
      const std::uint32_t tr = static_cast<std::uint32_t>((z + 1) * mesh_grid_size + x + 1);
      if (!options.alternating_diagonal_split || split45(x, z)) {
        appendTerrainTriangle(mesh, bl, tr, br, options.clip_ellipses,
                              options.clip_oriented_ellipses, options.clip_boxes);
        appendTerrainTriangle(mesh, bl, tl, tr, options.clip_ellipses,
                              options.clip_oriented_ellipses, options.clip_boxes);
      } else {
        appendTerrainTriangle(mesh, br, bl, tl, options.clip_ellipses,
                              options.clip_oriented_ellipses, options.clip_boxes);
        appendTerrainTriangle(mesh, br, tl, tr, options.clip_ellipses,
                              options.clip_oriented_ellipses, options.clip_boxes);
      }
    }
  }

  return mesh;
}

} // namespace aster
