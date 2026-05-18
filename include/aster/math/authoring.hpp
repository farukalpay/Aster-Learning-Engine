// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/bounds.hpp"
#include "aster/math/predicates.hpp"
#include "aster/math/tangent_space.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace aster {

enum class TransformSpace {
  Local,
  Parent,
  World,
  View,
  Screen,
};

enum class PivotMode {
  Median,
  BoundsCenter,
  ActiveElement,
  WorldOrigin,
};

struct TransformOrientation {
  Vec3 x{1.0f, 0.0f, 0.0f};
  Vec3 y{0.0f, 1.0f, 0.0f};
  Vec3 z{0.0f, 0.0f, 1.0f};
};

struct ManipulationPlane {
  Plane3 plane{};
  TransformSpace space = TransformSpace::World;
};

struct GizmoRayHit {
  bool hit = false;
  Vec3 position{};
  float distance = 0.0f;
  TransformOrientation orientation{};
};

struct SnapGrid {
  Vec3 origin{};
  Vec3 step{1.0f, 1.0f, 1.0f};
  bool enabled = true;
};

[[nodiscard]] inline Vec3 snapPoint(const Vec3 point, const SnapGrid grid) {
  if (!grid.enabled) {
    return point;
  }
  const Vec3 safe_step{std::max(std::abs(grid.step.x), 0.000001f),
                       std::max(std::abs(grid.step.y), 0.000001f),
                       std::max(std::abs(grid.step.z), 0.000001f)};
  const Vec3 local = point - grid.origin;
  return grid.origin + Vec3{std::round(local.x / safe_step.x) * safe_step.x,
                            std::round(local.y / safe_step.y) * safe_step.y,
                            std::round(local.z / safe_step.z) * safe_step.z};
}

[[nodiscard]] inline GizmoRayHit intersectManipulationPlane(const Ray3 ray,
                                                            const ManipulationPlane plane) {
  const RayHit3 hit = intersectRayPlane(ray.origin, normalizeOr(ray.direction, {0.0f, 0.0f, -1.0f}),
                                        plane.plane);
  if (!hit.hit) {
    return {};
  }
  return {.hit = true, .position = hit.point, .distance = hit.distance};
}

struct MeshMeasure {
  float surface_area = 0.0f;
  float signed_volume = 0.0f;
  float min_triangle_quality = 1.0f;
  float max_uv_stretch = 0.0f;
  Aabb3 bounds{};
};

struct AuthoringDiagnostic {
  MathError error = MathError::None;
  std::string message;
  std::size_t primitive_index = 0u;
};

struct AuthoringDiagnostics {
  std::vector<AuthoringDiagnostic> events;

  [[nodiscard]] bool ok() const {
    return events.empty();
  }
};

[[nodiscard]] inline float triangleArea(const Vec3 a, const Vec3 b, const Vec3 c) {
  return length(cross(b - a, c - a)) * 0.5f;
}

[[nodiscard]] inline float triangleQuality(const Vec3 a, const Vec3 b, const Vec3 c) {
  const float ab = distance(a, b);
  const float bc = distance(b, c);
  const float ca = distance(c, a);
  const float denom = ab * ab + bc * bc + ca * ca;
  if (denom <= 0.000001f) {
    return 0.0f;
  }
  return clamp((4.0f * std::sqrt(3.0f) * triangleArea(a, b, c)) / denom, 0.0f, 1.0f);
}

[[nodiscard]] inline MeshMeasure measureTriangleMesh(std::span<const Vec3> positions,
                                                     std::span<const std::uint32_t> indices,
                                                     std::span<const Vec2> uvs = {}) {
  MeshMeasure measure;
  if (positions.empty()) {
    return measure;
  }
  measure.bounds = {positions.front(), positions.front()};
  for (const Vec3 position : positions) {
    measure.bounds.min = min(measure.bounds.min, position);
    measure.bounds.max = max(measure.bounds.max, position);
  }
  measure.min_triangle_quality = 1.0f;
  for (std::size_t i = 0; i + 2u < indices.size(); i += 3u) {
    const std::uint32_t ia = indices[i + 0u];
    const std::uint32_t ib = indices[i + 1u];
    const std::uint32_t ic = indices[i + 2u];
    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
      continue;
    }
    const Vec3 a = positions[ia];
    const Vec3 b = positions[ib];
    const Vec3 c = positions[ic];
    measure.surface_area += triangleArea(a, b, c);
    measure.signed_volume += dot(a, cross(b, c)) / 6.0f;
    measure.min_triangle_quality = std::min(measure.min_triangle_quality, triangleQuality(a, b, c));
    if (!uvs.empty() && ia < uvs.size() && ib < uvs.size() && ic < uvs.size()) {
      const float world_area = std::max(triangleArea(a, b, c), 0.000001f);
      const float uv_area = std::abs(orient2dValue(uvs[ia], uvs[ib], uvs[ic])) * 0.5f;
      measure.max_uv_stretch = std::max(measure.max_uv_stretch, uv_area / world_area);
    }
  }
  return measure;
}

struct PlacementFilter {
  float min_normal_y = 0.0f;
  float max_slope_radians = pi() * 0.5f;
};

[[nodiscard]] inline bool acceptsPlacementNormal(const Vec3 normal, const PlacementFilter filter) {
  const Vec3 n = normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  const float slope = std::acos(clamp(n.y, -1.0f, 1.0f));
  return n.y >= filter.min_normal_y && slope <= filter.max_slope_radians;
}

} // namespace aster
