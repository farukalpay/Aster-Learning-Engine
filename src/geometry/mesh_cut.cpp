// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/geometry/mesh_cut.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace aster {
namespace {

constexpr float kMinimumAxisLength = 0.0001f;
constexpr int kMaximumPartitionPlanes = 16;

struct RuntimeCutVec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct RuntimeCutMeasure {
  RuntimeCutVec3 centroid{};
  RuntimeCutVec3 bounds_min{};
  RuntimeCutVec3 bounds_max{};
  float surface_area = 0.0f;
  float signed_volume = 0.0f;
};

extern "C" std::uint32_t
aster_runtime_measure_mesh_cut(const RuntimeCutVec3 *vertices, std::size_t vertex_count,
                               const std::uint32_t *indices, std::size_t index_count,
                               RuntimeCutMeasure *out_measure);

struct CutPolygon {
  std::vector<Vec3> vertices;
};

struct CutWorkFragment {
  std::vector<CutPolygon> polygons;
  std::uint32_t plane_mask = 0u;
  MeshCutFragmentSide side = MeshCutFragmentSide::Boundary;
};

struct CutMeasure {
  Vec3 centroid{};
  Vec3 bounds_min{};
  Vec3 bounds_max{};
  float surface_area = 0.0f;
  float signed_volume = 0.0f;
};

enum class PlaneClassification {
  Coplanar,
  Negative,
  Positive,
  Spanning,
};

Vec3 toVec3(const RuntimeCutVec3 value) {
  return {value.x, value.y, value.z};
}

RuntimeCutVec3 toRuntimeVec3(const Vec3 value) {
  return {value.x, value.y, value.z};
}

Vec3 safeNormalize(const Vec3 value, const Vec3 fallback) {
  const Vec3 normalized = normalize(value);
  return length(normalized) > kMinimumAxisLength ? normalized : fallback;
}

std::array<Vec3, 3> orthonormalAxes(const ConvexCutVolume &volume) {
  Vec3 axis_x = safeNormalize(volume.axis_x, {1.0f, 0.0f, 0.0f});
  Vec3 axis_y = volume.axis_y - axis_x * dot(volume.axis_y, axis_x);
  axis_y = safeNormalize(axis_y, {0.0f, 1.0f, 0.0f});
  Vec3 axis_z = safeNormalize(cross(axis_x, axis_y),
                              safeNormalize(volume.axis_z, {0.0f, 0.0f, 1.0f}));
  axis_y = safeNormalize(cross(axis_z, axis_x), axis_y);
  return {axis_x, axis_y, axis_z};
}

Vec3 positiveHalfExtents(const Vec3 half_extents) {
  return {std::max(half_extents.x, 0.001f), std::max(half_extents.y, 0.001f),
          std::max(half_extents.z, 0.001f)};
}

Vec3 largestHalfExtentVector(const Vec3 half_extents) {
  const float largest = std::max(std::max(half_extents.x, half_extents.y), half_extents.z);
  return {largest, largest, largest};
}

float signedDistance(const MeshCutPlane &plane, const Vec3 point) {
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

void appendUniquePoint(std::vector<Vec3> &points, const Vec3 point, const float epsilon) {
  for (const Vec3 existing : points) {
    if (length(existing - point) <= epsilon) {
      return;
    }
  }
  points.push_back(point);
}

void appendCleanPoint(std::vector<Vec3> &points, const Vec3 point, const float epsilon) {
  if (!points.empty() && length(points.back() - point) <= epsilon) {
    return;
  }
  points.push_back(point);
}

void closeCleanPolygon(std::vector<Vec3> &points, const float epsilon) {
  if (points.size() >= 2u && length(points.front() - points.back()) <= epsilon) {
    points.pop_back();
  }
}

Vec3 intersectSegmentPlane(const Vec3 a, const Vec3 b, const float da, const float db,
                           const float epsilon) {
  const float denom = da - db;
  const float t = std::abs(denom) > epsilon ? clamp(da / denom, 0.0f, 1.0f) : 0.5f;
  return a + (b - a) * t;
}

Vec3 newellNormal(const std::vector<Vec3> &points) {
  Vec3 normal{};
  for (std::size_t i = 0; i < points.size(); ++i) {
    const Vec3 current = points[i];
    const Vec3 next = points[(i + 1u) % points.size()];
    normal.x += (current.y - next.y) * (current.z + next.z);
    normal.y += (current.z - next.z) * (current.x + next.x);
    normal.z += (current.x - next.x) * (current.y + next.y);
  }
  return normal;
}

Vec3 faceNormal(const Vec3 a, const Vec3 b, const Vec3 c) {
  return safeNormalize(cross(b - a, c - a), {0.0f, 1.0f, 0.0f});
}

void sortCapPolygon(std::vector<Vec3> &points, const Vec3 normal, const float epsilon) {
  if (points.size() < 3u) {
    return;
  }
  std::vector<Vec3> unique_points;
  unique_points.reserve(points.size());
  for (const Vec3 point : points) {
    appendUniquePoint(unique_points, point, epsilon);
  }
  points = std::move(unique_points);
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

  if (dot(newellNormal(points), normal) < 0.0f) {
    std::reverse(points.begin(), points.end());
  }
}

std::vector<Vec3> clipPolygonToHalfspace(const CutPolygon &polygon, const MeshCutPlane &plane,
                                         const bool keep_positive, const float plane_epsilon,
                                         const float point_epsilon,
                                         std::vector<Vec3> &cap_points) {
  std::vector<Vec3> clipped;
  if (polygon.vertices.empty()) {
    return clipped;
  }
  clipped.reserve(polygon.vertices.size() + 1u);

  Vec3 previous = polygon.vertices.back();
  float previous_distance = signedDistance(plane, previous);
  bool previous_inside =
      keep_positive ? previous_distance >= -plane_epsilon : previous_distance <= plane_epsilon;
  for (const Vec3 current : polygon.vertices) {
    const float current_distance = signedDistance(plane, current);
    const bool current_inside =
        keep_positive ? current_distance >= -plane_epsilon : current_distance <= plane_epsilon;
    if (previous_inside != current_inside) {
      const Vec3 intersection =
          intersectSegmentPlane(previous, current, previous_distance, current_distance,
                                plane_epsilon);
      appendCleanPoint(clipped, intersection, point_epsilon);
      appendUniquePoint(cap_points, intersection, point_epsilon);
    }
    if (current_inside) {
      appendCleanPoint(clipped, current, point_epsilon);
    }
    previous = current;
    previous_distance = current_distance;
    previous_inside = current_inside;
  }
  closeCleanPolygon(clipped, point_epsilon);
  return clipped;
}

PlaneClassification classifyPolygons(const std::vector<CutPolygon> &polygons,
                                      const MeshCutPlane &plane, const float epsilon) {
  bool has_negative = false;
  bool has_positive = false;
  for (const CutPolygon &polygon : polygons) {
    for (const Vec3 point : polygon.vertices) {
      const float distance = signedDistance(plane, point);
      has_negative = has_negative || distance < -epsilon;
      has_positive = has_positive || distance > epsilon;
      if (has_negative && has_positive) {
        return PlaneClassification::Spanning;
      }
    }
  }
  if (has_negative) {
    return PlaneClassification::Negative;
  }
  if (has_positive) {
    return PlaneClassification::Positive;
  }
  return PlaneClassification::Coplanar;
}

std::vector<CutPolygon> clipPolygonsToHalfspace(const std::vector<CutPolygon> &polygons,
                                                const MeshCutPlane &plane,
                                                const bool keep_positive,
                                                const MeshCutSpec &spec,
                                                std::vector<Vec3> &seam_points) {
  std::vector<CutPolygon> clipped_polygons;
  clipped_polygons.reserve(polygons.size() + 1u);
  std::vector<Vec3> cap_points;
  for (const CutPolygon &polygon : polygons) {
    std::vector<Vec3> clipped =
        clipPolygonToHalfspace(polygon, plane, keep_positive, spec.plane_epsilon,
                               spec.point_merge_epsilon, cap_points);
    if (clipped.size() >= 3u) {
      clipped_polygons.push_back({std::move(clipped)});
    }
  }

  if (spec.seal_cuts && cap_points.size() >= 3u) {
    const Vec3 cap_normal = keep_positive ? plane.normal * -1.0f : plane.normal;
    sortCapPolygon(cap_points, cap_normal, spec.point_merge_epsilon);
    if (cap_points.size() >= 3u) {
      for (const Vec3 point : cap_points) {
        appendUniquePoint(seam_points, point, spec.point_merge_epsilon);
      }
      clipped_polygons.push_back({std::move(cap_points)});
    }
  }
  return clipped_polygons;
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
  Vec3 normal = safeNormalize(newellNormal(points), faceNormal(points[0], points[1], points[2]));
  for (std::size_t i = 1u; i + 1u < points.size(); ++i) {
    appendTriangle(mesh, points[0], points[i], points[i + 1u], normal);
  }
}

std::vector<CutPolygon> polygonsFromMesh(const CpuMesh &mesh) {
  std::vector<CutPolygon> polygons;
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

CpuMesh meshFromPolygons(const std::vector<CutPolygon> &polygons) {
  CpuMesh mesh;
  std::size_t polygon_vertex_count = 0u;
  for (const CutPolygon &polygon : polygons) {
    polygon_vertex_count += polygon.vertices.size();
  }
  mesh.vertices.reserve(polygon_vertex_count);
  mesh.indices.reserve(polygon_vertex_count * 3u);
  for (const CutPolygon &polygon : polygons) {
    appendPolygon(mesh, polygon.vertices);
  }
  return mesh;
}

CutMeasure measureMesh(const CpuMesh &mesh) {
  std::vector<RuntimeCutVec3> positions;
  positions.reserve(mesh.vertices.size());
  for (const Vertex &vertex : mesh.vertices) {
    positions.push_back(toRuntimeVec3(vertex.position));
  }

  RuntimeCutMeasure runtime_measure{};
  const std::uint32_t measured = aster_runtime_measure_mesh_cut(
      positions.data(), positions.size(), mesh.indices.data(), mesh.indices.size(),
      &runtime_measure);
  if (measured == 0u) {
    return {};
  }
  return {.centroid = toVec3(runtime_measure.centroid),
          .bounds_min = toVec3(runtime_measure.bounds_min),
          .bounds_max = toVec3(runtime_measure.bounds_max),
          .surface_area = runtime_measure.surface_area,
          .signed_volume = runtime_measure.signed_volume};
}

Vec3 centerFromBounds(const Vec3 bounds_min, const Vec3 bounds_max) {
  return (bounds_min + bounds_max) * 0.5f;
}

MeshCutFragment makeFragment(CpuMesh mesh, const CutWorkFragment &work, const Vec3 source_center,
                             const MeshCutSpec &spec) {
  const CutMeasure measure = measureMesh(mesh);
  if (measure.surface_area <= spec.minimum_fragment_area) {
    return {};
  }
  Vec3 impulse = measure.centroid - source_center;
  if (length(impulse) <= kMinimumAxisLength) {
    impulse = {0.0f, 1.0f, 0.0f};
  }
  return {.mesh = std::move(mesh),
          .centroid = measure.centroid,
          .bounds_min = measure.bounds_min,
          .bounds_max = measure.bounds_max,
          .impulse_direction = safeNormalize(impulse, {0.0f, 1.0f, 0.0f}),
          .surface_area = measure.surface_area,
          .signed_volume = measure.signed_volume,
          .side = work.side,
          .cut_plane_mask = work.plane_mask};
}

void appendFragment(MeshCutResult &result, const CutWorkFragment &work, const Vec3 source_center,
                    const MeshCutSpec &spec) {
  CpuMesh mesh = meshFromPolygons(work.polygons);
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    return;
  }
  MeshCutFragment fragment = makeFragment(std::move(mesh), work, source_center, spec);
  if (fragment.surface_area > spec.minimum_fragment_area) {
    result.fragments.push_back(std::move(fragment));
  }
}

MeshCutResult clipToSingleSide(const CpuMesh &source_mesh, const MeshCutSpec &spec,
                               const bool keep_positive) {
  MeshCutResult result;
  std::vector<CutPolygon> polygons = polygonsFromMesh(source_mesh);
  if (polygons.empty()) {
    return result;
  }
  for (const MeshCutPlane &plane : spec.planes) {
    polygons = clipPolygonsToHalfspace(polygons, plane, keep_positive, spec, result.seam_points);
    if (polygons.empty()) {
      return result;
    }
  }
  CutWorkFragment work{std::move(polygons), 0u,
                       keep_positive ? MeshCutFragmentSide::Positive
                                     : MeshCutFragmentSide::Negative};
  const CutMeasure source_measure = measureMesh(source_mesh);
  appendFragment(result, work, source_measure.centroid, spec);
  return result;
}

void appendBoxFace(CpuMesh &mesh, const std::array<Vec3, 8> &corner,
                   const std::array<int, 4> face, const Vec3 outward) {
  appendTriangle(mesh, corner[face[0]], corner[face[1]], corner[face[2]], outward);
  appendTriangle(mesh, corner[face[0]], corner[face[2]], corner[face[3]], outward);
}

Vec3 volumePoint(const ConvexCutVolume &volume, const Vec3 local_unit) {
  const std::array<Vec3, 3> axes = orthonormalAxes(volume);
  const Vec3 half = positiveHalfExtents(volume.half_extents);
  return volume.center + axes[0] * (local_unit.x * half.x) +
         axes[1] * (local_unit.y * half.y) + axes[2] * (local_unit.z * half.z);
}

} // namespace

MeshCutPlane makeMeshCutPlane(const Vec3 point, Vec3 normal) {
  normal = safeNormalize(normal, {0.0f, 1.0f, 0.0f});
  return {.point = point, .normal = normal};
}

CpuMesh makeConvexCutBox(const ConvexCutVolume &volume) {
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

std::vector<MeshCutPlane> buildImpactCutPlanes(const MeshCutImpactSpec &spec) {
  const std::array<Vec3, 3> volume_axes = orthonormalAxes(spec.volume);
  const Vec3 half = positiveHalfExtents(spec.volume.half_extents);
  Vec3 impact_normal = safeNormalize(spec.impact_normal, volume_axes[2]);
  Vec3 tangent_x = volume_axes[0] - impact_normal * dot(volume_axes[0], impact_normal);
  tangent_x = safeNormalize(tangent_x, safeNormalize(cross(volume_axes[1], impact_normal),
                                                     {1.0f, 0.0f, 0.0f}));
  Vec3 tangent_y = safeNormalize(cross(impact_normal, tangent_x), volume_axes[1]);
  const Vec3 scale = largestHalfExtentVector(half);
  const float offset_scale = std::max(std::max(scale.x, scale.y), scale.z);
  const int plane_count = std::clamp(spec.plane_count, 1, kMaximumPartitionPlanes);

  std::uint32_t state = spec.seed == 0u ? 1u : spec.seed;
  std::vector<MeshCutPlane> planes;
  planes.reserve(static_cast<std::size_t>(plane_count));
  for (int i = 0; i < plane_count; ++i) {
    const float fill =
        plane_count <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(plane_count - 1);
    const float angle = (hashUnit(state) + fill) * 6.28318530717958647692f;
    const Vec3 tangent_direction =
        tangent_x * std::cos(angle) + tangent_y * std::sin(angle);
    const float angular_scale = clamp(spec.angular_spread, 0.0f, 1.0f);
    Vec3 normal = safeNormalize(impact_normal + tangent_direction * (hashSigned(state) *
                                                                     angular_scale),
                                impact_normal);
    if ((i % 3) == 1) {
      normal = safeNormalize(tangent_direction + impact_normal * (0.25f + angular_scale * 0.35f),
                             tangent_direction);
    }
    const float offset = hashSigned(state) * clamp(spec.offset_spread, 0.0f, 1.0f) * offset_scale;
    const Vec3 radial_offset =
        tangent_direction * (hashSigned(state) * clamp(spec.offset_spread, 0.0f, 1.0f) *
                             offset_scale * 0.55f);
    const Vec3 local_hint{clamp(hashSigned(state), -0.92f, 0.92f),
                          clamp(hashSigned(state), -0.92f, 0.92f),
                          clamp(0.35f - fill * 0.70f, -0.85f, 0.85f)};
    const Vec3 point = (volumePoint(spec.volume, local_hint) + spec.impact_point) * 0.5f +
                       impact_normal * offset + radial_offset;
    planes.push_back(makeMeshCutPlane(point, normal));
  }
  return planes;
}

MeshCutResult partitionMeshByCutPlanes(const CpuMesh &source_mesh, const MeshCutSpec &spec) {
  if (source_mesh.vertices.empty() || source_mesh.indices.empty() || spec.planes.empty()) {
    return {};
  }
  if (spec.operation == MeshCutOperation::KeepNegativeHalfspaces) {
    return clipToSingleSide(source_mesh, spec, false);
  }
  if (spec.operation == MeshCutOperation::KeepPositiveHalfspaces) {
    return clipToSingleSide(source_mesh, spec, true);
  }

  const std::vector<CutPolygon> source_polygons = polygonsFromMesh(source_mesh);
  if (source_polygons.empty()) {
    return {};
  }
  const CutMeasure source_measure = measureMesh(source_mesh);
  const Vec3 source_center = source_measure.surface_area > 0.0f
                                 ? source_measure.centroid
                                 : centerFromBounds(source_measure.bounds_min,
                                                    source_measure.bounds_max);

  std::vector<CutWorkFragment> work;
  work.push_back({source_polygons, 0u, MeshCutFragmentSide::Boundary});
  MeshCutResult result;
  const std::size_t plane_count =
      std::min<std::size_t>(spec.planes.size(), static_cast<std::size_t>(kMaximumPartitionPlanes));
  for (std::size_t plane_index = 0u; plane_index < plane_count; ++plane_index) {
    const MeshCutPlane plane = makeMeshCutPlane(spec.planes[plane_index].point,
                                                spec.planes[plane_index].normal);
    std::vector<CutWorkFragment> next_work;
    next_work.reserve(work.size() * 2u);
    for (const CutWorkFragment &fragment : work) {
      const PlaneClassification classification =
          classifyPolygons(fragment.polygons, plane, spec.plane_epsilon);
      if (classification == PlaneClassification::Coplanar) {
        next_work.push_back(fragment);
        continue;
      }
      if (classification == PlaneClassification::Negative ||
          classification == PlaneClassification::Positive) {
        CutWorkFragment carried = fragment;
        carried.side = classification == PlaneClassification::Negative
                           ? MeshCutFragmentSide::Negative
                           : MeshCutFragmentSide::Positive;
        next_work.push_back(std::move(carried));
        continue;
      }

      const std::uint32_t plane_bit =
          plane_index < 32u ? (1u << static_cast<std::uint32_t>(plane_index)) : 0u;
      std::vector<CutPolygon> negative =
          clipPolygonsToHalfspace(fragment.polygons, plane, false, spec, result.seam_points);
      if (!negative.empty()) {
        next_work.push_back({std::move(negative), fragment.plane_mask | plane_bit,
                             MeshCutFragmentSide::Negative});
      }
      std::vector<CutPolygon> positive =
          clipPolygonsToHalfspace(fragment.polygons, plane, true, spec, result.seam_points);
      if (!positive.empty()) {
        next_work.push_back({std::move(positive), fragment.plane_mask | plane_bit,
                             MeshCutFragmentSide::Positive});
      }
    }
    work = std::move(next_work);
    if (work.empty()) {
      return result;
    }
  }

  result.fragments.reserve(work.size());
  for (const CutWorkFragment &fragment : work) {
    appendFragment(result, fragment, source_center, spec);
  }
  return result;
}

MeshCutResult buildImpactMeshCut(const MeshCutImpactSpec &spec) {
  MeshCutSpec cut_spec;
  cut_spec.planes = buildImpactCutPlanes(spec);
  cut_spec.operation = MeshCutOperation::Partition;
  cut_spec.minimum_fragment_area = spec.minimum_fragment_area;
  MeshCutResult result = partitionMeshByCutPlanes(makeConvexCutBox(spec.volume), cut_spec);
  const Vec3 impact_normal = safeNormalize(spec.impact_normal, {0.0f, 1.0f, 0.0f});
  for (MeshCutFragment &fragment : result.fragments) {
    const Vec3 impact_delta = fragment.centroid - spec.impact_point;
    fragment.impulse_direction =
        safeNormalize(impact_delta + impact_normal * 0.42f, impact_normal);
  }
  return result;
}

} // namespace aster
