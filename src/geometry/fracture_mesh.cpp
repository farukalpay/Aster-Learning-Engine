// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/fracture_mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace aster {
namespace {

constexpr float kPointEpsilon = 0.0005f;
constexpr float kPlaneEpsilon = 0.00001f;
constexpr float kMinimumShardArea = 0.0001f;

struct FracturePlane {
  Vec3 normal{0.0f, 1.0f, 0.0f};
  Vec3 point{};
};

struct FracturePolygon {
  std::vector<Vec3> vertices;
};

struct ShardMeasure {
  Vec3 centroid{};
  float area = 0.0f;
};

Vec3 safeNormalize(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > 0.0001f ? normalized : fallback;
}

std::array<Vec3, 3> orthonormalAxes(const ConvexFractureVolume &volume) {
  Vec3 axis_x = safeNormalize(volume.axis_x, {1.0f, 0.0f, 0.0f});
  Vec3 axis_y = volume.axis_y - axis_x * dot(volume.axis_y, axis_x);
  axis_y = safeNormalize(axis_y, {0.0f, 1.0f, 0.0f});
  Vec3 axis_z = cross(axis_x, axis_y);
  axis_z = safeNormalize(axis_z, safeNormalize(volume.axis_z, {0.0f, 0.0f, 1.0f}));
  axis_y = safeNormalize(cross(axis_z, axis_x), axis_y);
  return {axis_x, axis_y, axis_z};
}

Vec3 positiveHalfExtents(const Vec3 half_extents) {
  return {std::max(half_extents.x, 0.001f), std::max(half_extents.y, 0.001f),
          std::max(half_extents.z, 0.001f)};
}

Vec3 volumePoint(const ConvexFractureVolume &volume, const Vec3 local_unit) {
  const std::array<Vec3, 3> axes = orthonormalAxes(volume);
  const Vec3 half = positiveHalfExtents(volume.half_extents);
  return volume.center + axes[0] * (local_unit.x * half.x) +
         axes[1] * (local_unit.y * half.y) + axes[2] * (local_unit.z * half.z);
}

Vec3 volumeLocalUnit(const ConvexFractureVolume &volume, const Vec3 point) {
  const std::array<Vec3, 3> axes = orthonormalAxes(volume);
  const Vec3 half = positiveHalfExtents(volume.half_extents);
  const Vec3 delta = point - volume.center;
  return {clamp(dot(delta, axes[0]) / half.x, -1.0f, 1.0f),
          clamp(dot(delta, axes[1]) / half.y, -1.0f, 1.0f),
          clamp(dot(delta, axes[2]) / half.z, -1.0f, 1.0f)};
}

float signedDistance(const FracturePlane &plane, const Vec3 point) {
  return dot(point - plane.point, plane.normal);
}

std::uint32_t advanceHash(std::uint32_t &state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

float hashUnit(std::uint32_t &state) {
  return static_cast<float>((advanceHash(state) >> 8u) & 0x00ffffffu) /
         static_cast<float>(0x01000000u);
}

float hashSigned(std::uint32_t &state) {
  return hashUnit(state) * 2.0f - 1.0f;
}

Vec3 randomUnitVector(std::uint32_t &state) {
  Vec3 value{hashSigned(state), hashSigned(state), hashSigned(state)};
  if (length(value) <= 0.0001f) {
    value = {1.0f, 0.0f, 0.0f};
  }
  return normalize(value);
}

void appendUniquePoint(std::vector<Vec3> &points, const Vec3 point) {
  for (const Vec3 existing : points) {
    if (length(existing - point) <= kPointEpsilon) {
      return;
    }
  }
  points.push_back(point);
}

Vec3 intersectSegmentPlane(const Vec3 a, const Vec3 b, const float da, const float db) {
  const float denom = da - db;
  const float t = std::abs(denom) > kPlaneEpsilon ? clamp(da / denom, 0.0f, 1.0f) : 0.5f;
  return a + (b - a) * t;
}

std::vector<Vec3> clipPolygon(const FracturePolygon &polygon, const FracturePlane &plane,
                              std::vector<Vec3> &cap_points) {
  std::vector<Vec3> clipped;
  if (polygon.vertices.empty()) {
    return clipped;
  }
  clipped.reserve(polygon.vertices.size() + 1u);

  Vec3 previous = polygon.vertices.back();
  float previous_distance = signedDistance(plane, previous);
  bool previous_inside = previous_distance <= kPlaneEpsilon;
  for (const Vec3 current : polygon.vertices) {
    const float current_distance = signedDistance(plane, current);
    const bool current_inside = current_distance <= kPlaneEpsilon;
    if (previous_inside != current_inside) {
      const Vec3 intersection =
          intersectSegmentPlane(previous, current, previous_distance, current_distance);
      clipped.push_back(intersection);
      appendUniquePoint(cap_points, intersection);
    }
    if (current_inside) {
      clipped.push_back(current);
    }
    previous = current;
    previous_distance = current_distance;
    previous_inside = current_inside;
  }
  return clipped;
}

void sortCapPolygon(std::vector<Vec3> &points, const Vec3 normal) {
  if (points.size() < 3u) {
    return;
  }
  Vec3 center{};
  for (const Vec3 point : points) {
    center = center + point;
  }
  center = center / static_cast<float>(points.size());

  const Vec3 reference =
      std::abs(normal.y) < 0.90f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  const Vec3 axis_u = safeNormalize(cross(reference, normal), {1.0f, 0.0f, 0.0f});
  const Vec3 axis_v = safeNormalize(cross(normal, axis_u), {0.0f, 0.0f, 1.0f});
  std::sort(points.begin(), points.end(), [&](const Vec3 lhs, const Vec3 rhs) {
    const Vec3 lhs_delta = lhs - center;
    const Vec3 rhs_delta = rhs - center;
    return std::atan2(dot(lhs_delta, axis_v), dot(lhs_delta, axis_u)) <
           std::atan2(dot(rhs_delta, axis_v), dot(rhs_delta, axis_u));
  });

  Vec3 polygon_normal{};
  for (std::size_t i = 0; i < points.size(); ++i) {
    const Vec3 current = points[i];
    const Vec3 next = points[(i + 1u) % points.size()];
    polygon_normal.x += (current.y - next.y) * (current.z + next.z);
    polygon_normal.y += (current.z - next.z) * (current.x + next.x);
    polygon_normal.z += (current.x - next.x) * (current.y + next.y);
  }
  if (dot(polygon_normal, normal) < 0.0f) {
    std::reverse(points.begin(), points.end());
  }
}

std::vector<FracturePolygon> clipMeshByPlane(const std::vector<FracturePolygon> &polygons,
                                             const FracturePlane &plane) {
  std::vector<FracturePolygon> clipped_polygons;
  clipped_polygons.reserve(polygons.size() + 1u);
  std::vector<Vec3> cap_points;
  for (const FracturePolygon &polygon : polygons) {
    std::vector<Vec3> clipped = clipPolygon(polygon, plane, cap_points);
    if (clipped.size() >= 3u) {
      clipped_polygons.push_back({std::move(clipped)});
    }
  }
  if (cap_points.size() >= 3u) {
    sortCapPolygon(cap_points, plane.normal);
    clipped_polygons.push_back({std::move(cap_points)});
  }
  return clipped_polygons;
}

Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return safeNormalize(cross(b - a, c - a), {0.0f, 1.0f, 0.0f});
}

void appendTriangle(CpuMesh &mesh, Vec3 a, Vec3 b, Vec3 c, const Vec3 preferred_normal) {
  Vec3 normal = faceNormal(a, b, c);
  if (dot(normal, preferred_normal) < 0.0f) {
    std::swap(b, c);
    normal = faceNormal(a, b, c);
  }
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, {0.0f, 0.0f}});
  mesh.vertices.push_back({b, normal, {1.0f, 0.0f}});
  mesh.vertices.push_back({c, normal, {0.5f, 1.0f}});
  mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u});
}

void appendPolygon(CpuMesh &mesh, const std::vector<Vec3> &points) {
  if (points.size() < 3u) {
    return;
  }
  Vec3 normal{};
  for (std::size_t i = 0; i < points.size(); ++i) {
    const Vec3 current = points[i];
    const Vec3 next = points[(i + 1u) % points.size()];
    normal.x += (current.y - next.y) * (current.z + next.z);
    normal.y += (current.z - next.z) * (current.x + next.x);
    normal.z += (current.x - next.x) * (current.y + next.y);
  }
  normal = safeNormalize(normal, faceNormal(points[0], points[1], points[2]));
  for (std::size_t i = 1; i + 1u < points.size(); ++i) {
    appendTriangle(mesh, points[0], points[i], points[i + 1u], normal);
  }
}

std::vector<FracturePolygon> polygonsFromMesh(const CpuMesh &mesh) {
  std::vector<FracturePolygon> polygons;
  polygons.reserve(mesh.indices.size() / 3u);
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const std::uint32_t ia = mesh.indices[i];
    const std::uint32_t ib = mesh.indices[i + 1u];
    const std::uint32_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
      continue;
    }
    polygons.push_back({{mesh.vertices[ia].position, mesh.vertices[ib].position,
                         mesh.vertices[ic].position}});
  }
  return polygons;
}

CpuMesh meshFromPolygons(const std::vector<FracturePolygon> &polygons) {
  CpuMesh mesh;
  std::size_t vertex_count = 0u;
  for (const FracturePolygon &polygon : polygons) {
    vertex_count += polygon.vertices.size();
  }
  mesh.vertices.reserve(vertex_count);
  mesh.indices.reserve(vertex_count * 3u);
  for (const FracturePolygon &polygon : polygons) {
    appendPolygon(mesh, polygon.vertices);
  }
  return mesh;
}

float triangleArea(const Vec3 a, const Vec3 b, const Vec3 c) {
  return length(cross(b - a, c - a)) * 0.5f;
}

ShardMeasure measureShard(const CpuMesh &mesh) {
  ShardMeasure measure;
  for (std::size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
    const Vec3 a = mesh.vertices[mesh.indices[i]].position;
    const Vec3 b = mesh.vertices[mesh.indices[i + 1u]].position;
    const Vec3 c = mesh.vertices[mesh.indices[i + 2u]].position;
    const float area = triangleArea(a, b, c);
    measure.centroid = measure.centroid + (a + b + c) * (area / 3.0f);
    measure.area += area;
  }
  if (measure.area > kMinimumShardArea) {
    measure.centroid = measure.centroid / measure.area;
  }
  return measure;
}

std::vector<Vec3> generatedSeedPoints(const VoronoiFractureSpec &spec) {
  if (!spec.seed_points.empty()) {
    return spec.seed_points;
  }

  const int shard_count = std::clamp(spec.shard_count, 2, 48);
  std::vector<Vec3> seeds;
  seeds.reserve(static_cast<std::size_t>(shard_count));
  std::uint32_t state = spec.seed == 0u ? 1u : spec.seed;
  const Vec3 impact_local = volumeLocalUnit(spec.volume, spec.impact_point);
  const float biased_count =
      static_cast<float>(shard_count) * clamp(spec.impact_seed_fraction, 0.0f, 1.0f);
  const float random_spread = clamp(spec.random_seed_spread, 0.0f, 1.0f);
  const float impact_spread = clamp(spec.impact_seed_spread, 0.0f, 1.0f);
  for (int i = 0; i < shard_count; ++i) {
    const bool impact_biased = static_cast<float>(i) < biased_count;
    const Vec3 random_local{hashSigned(state) * random_spread, hashSigned(state) * random_spread,
                            hashSigned(state) * random_spread};
    const Vec3 offset = randomUnitVector(state) * (impact_spread * hashUnit(state));
    const Vec3 local = impact_biased ? clamp(impact_local + offset, -0.96f, 0.96f)
                                     : clamp(random_local, -0.96f, 0.96f);
    seeds.push_back(volumePoint(spec.volume, local));
  }
  return seeds;
}

void appendBoxFace(CpuMesh &mesh, const std::array<Vec3, 8> &corner, const std::array<int, 4> face,
                   const Vec3 outward) {
  appendTriangle(mesh, corner[face[0]], corner[face[1]], corner[face[2]], outward);
  appendTriangle(mesh, corner[face[0]], corner[face[2]], corner[face[3]], outward);
}

} // namespace

CpuMesh makeConvexFractureBox(const ConvexFractureVolume &volume) {
  const std::array<Vec3, 3> axes = orthonormalAxes(volume);
  const Vec3 half = positiveHalfExtents(volume.half_extents);
  const Vec3 x = axes[0] * half.x;
  const Vec3 y = axes[1] * half.y;
  const Vec3 z = axes[2] * half.z;
  const std::array<Vec3, 8> corner{volume.center - x - y - z, volume.center + x - y - z,
                                   volume.center + x + y - z, volume.center - x + y - z,
                                   volume.center - x - y + z, volume.center + x - y + z,
                                   volume.center + x + y + z, volume.center - x + y + z};
  CpuMesh mesh;
  mesh.vertices.reserve(36u);
  mesh.indices.reserve(36u);
  appendBoxFace(mesh, corner, {1, 0, 3, 2}, axes[2] * -1.0f);
  appendBoxFace(mesh, corner, {4, 5, 6, 7}, axes[2]);
  appendBoxFace(mesh, corner, {0, 4, 7, 3}, axes[0] * -1.0f);
  appendBoxFace(mesh, corner, {5, 1, 2, 6}, axes[0]);
  appendBoxFace(mesh, corner, {3, 7, 6, 2}, axes[1]);
  appendBoxFace(mesh, corner, {0, 1, 5, 4}, axes[1] * -1.0f);
  return mesh;
}

std::vector<VoronoiFractureShard> buildVoronoiFracture(const CpuMesh &source_mesh,
                                                       const VoronoiFractureSpec &spec) {
  if (source_mesh.vertices.empty() || source_mesh.indices.empty()) {
    return {};
  }

  const std::vector<FracturePolygon> source_polygons = polygonsFromMesh(source_mesh);
  if (source_polygons.empty()) {
    return {};
  }

  const std::vector<Vec3> seeds = generatedSeedPoints(spec);
  if (seeds.size() < 2u) {
    return {};
  }

  std::vector<VoronoiFractureShard> shards;
  shards.reserve(seeds.size());
  float total_area = 0.0f;
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    std::vector<FracturePolygon> cell = source_polygons;
    for (std::size_t j = 0; j < seeds.size(); ++j) {
      if (i == j) {
        continue;
      }
      const Vec3 separator = seeds[j] - seeds[i];
      if (length(separator) <= 0.0001f) {
        continue;
      }
      const FracturePlane plane{normalize(separator), (seeds[i] + seeds[j]) * 0.5f};
      cell = clipMeshByPlane(cell, plane);
      if (cell.empty()) {
        break;
      }
    }
    if (cell.empty()) {
      continue;
    }
    CpuMesh mesh = meshFromPolygons(cell);
    if (mesh.vertices.empty() || mesh.indices.empty()) {
      continue;
    }
    const ShardMeasure measure = measureShard(mesh);
    if (measure.area <= kMinimumShardArea) {
      continue;
    }
    total_area += measure.area;
    Vec3 impulse = measure.centroid - spec.impact_point;
    impulse = impulse + safeNormalize(spec.impact_normal, {0.0f, 1.0f, 0.0f}) * 0.35f;
    shards.push_back({std::move(mesh), measure.centroid,
                      safeNormalize(impulse, safeNormalize(spec.impact_normal,
                                                           {0.0f, 1.0f, 0.0f})),
                      measure.area});
  }

  if (total_area > kMinimumShardArea) {
    for (VoronoiFractureShard &shard : shards) {
      shard.relative_volume = shard.relative_volume / total_area;
    }
  }
  return shards;
}

std::vector<VoronoiFractureShard> buildImpactVoronoiFracture(const VoronoiFractureSpec &spec) {
  return buildVoronoiFracture(makeConvexFractureBox(spec.volume), spec);
}

} // namespace aster
