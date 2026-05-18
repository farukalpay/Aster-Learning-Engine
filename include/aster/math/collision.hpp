// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/bounds.hpp"

#include <array>
#include <span>
#include <vector>

namespace aster {

struct ContactPoint3 {
  Vec3 position{};
  float penetration = 0.0f;
};

struct ContactManifold3 {
  bool hit = false;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float penetration = 0.0f;
  std::array<ContactPoint3, 4> points{};
  std::size_t point_count = 0u;
};

[[nodiscard]] inline ContactManifold3 manifoldAabbAabb(const Aabb3 lhs, const Aabb3 rhs) {
  if (!overlaps(lhs, rhs)) {
    return {};
  }
  const Vec3 lhs_center = center(lhs);
  const Vec3 rhs_center = center(rhs);
  const Vec3 lhs_half = halfExtents(lhs);
  const Vec3 rhs_half = halfExtents(rhs);
  const Vec3 delta = rhs_center - lhs_center;
  const Vec3 overlap_depth = lhs_half + rhs_half - Vec3{std::abs(delta.x), std::abs(delta.y),
                                                        std::abs(delta.z)};
  Vec3 normal{delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
  float penetration = overlap_depth.x;
  if (overlap_depth.y < penetration) {
    normal = {0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
    penetration = overlap_depth.y;
  }
  if (overlap_depth.z < penetration) {
    normal = {0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f};
    penetration = overlap_depth.z;
  }
  const Vec3 point = closestPoint(lhs, rhs_center);
  return {.hit = true,
          .normal = normal,
          .penetration = penetration,
          .points = {{{point, penetration}}},
          .point_count = 1u};
}

[[nodiscard]] inline Vec3 supportPoint(std::span<const Vec3> points, const Vec3 direction) {
  if (points.empty()) {
    return {};
  }
  Vec3 best = points.front();
  float best_dot = dot(best, direction);
  for (const Vec3 point : points.subspan(1)) {
    const float candidate = dot(point, direction);
    if (candidate > best_dot) {
      best = point;
      best_dot = candidate;
    }
  }
  return best;
}

[[nodiscard]] inline Vec3 supportMinkowski(std::span<const Vec3> lhs, std::span<const Vec3> rhs,
                                           const Vec3 direction) {
  return supportPoint(lhs, direction) - supportPoint(rhs, -direction);
}

[[nodiscard]] inline Vec3 tripleProduct(const Vec3 a, const Vec3 b, const Vec3 c) {
  return b * dot(c, a) - a * dot(c, b);
}

inline bool updateSimplex(std::vector<Vec3> &simplex, Vec3 &direction) {
  const Vec3 a = simplex.back();
  const Vec3 ao = -a;
  if (simplex.size() == 2u) {
    const Vec3 b = simplex[0u];
    const Vec3 ab = b - a;
    direction = tripleProduct(ab, ao, ab);
    if (lengthSquared(direction) <= 0.000001f) {
      direction = normalizeOr(cross(ab, {0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
    }
    return false;
  }
  if (simplex.size() == 3u) {
    const Vec3 b = simplex[1u];
    const Vec3 c = simplex[0u];
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 abc = cross(ab, ac);
    const Vec3 ab_perp = cross(ab, abc);
    if (dot(ab_perp, ao) > 0.0f) {
      simplex = {b, a};
      direction = ab_perp;
      return false;
    }
    const Vec3 ac_perp = cross(abc, ac);
    if (dot(ac_perp, ao) > 0.0f) {
      simplex = {c, a};
      direction = ac_perp;
      return false;
    }
    direction = dot(abc, ao) > 0.0f ? abc : -abc;
    return false;
  }
  if (simplex.size() == 4u) {
    const Vec3 b = simplex[2u];
    const Vec3 c = simplex[1u];
    const Vec3 d = simplex[0u];
    const Vec3 faces[3] = {cross(b - a, c - a), cross(c - a, d - a), cross(d - a, b - a)};
    const Vec3 points[3] = {b, c, d};
    for (int i = 0; i < 3; ++i) {
      Vec3 normal = faces[i];
      if (dot(normal, points[i] - a) < 0.0f) {
        normal = -normal;
      }
      if (dot(normal, ao) > 0.0f) {
        simplex = {i == 0 ? c : d, points[i], a};
        direction = normal;
        return false;
      }
    }
    return true;
  }
  return false;
}

[[nodiscard]] inline bool gjkIntersects(std::span<const Vec3> lhs, std::span<const Vec3> rhs,
                                        const int max_iterations = 32) {
  if (lhs.empty() || rhs.empty()) {
    return false;
  }
  Vec3 direction{1.0f, 0.0f, 0.0f};
  std::vector<Vec3> simplex;
  simplex.reserve(4u);
  simplex.push_back(supportMinkowski(lhs, rhs, direction));
  direction = -simplex.back();
  for (int i = 0; i < max_iterations; ++i) {
    const Vec3 point = supportMinkowski(lhs, rhs, direction);
    if (dot(point, direction) < 0.0f) {
      return false;
    }
    simplex.push_back(point);
    if (updateSimplex(simplex, direction)) {
      return true;
    }
  }
  recordMathDiagnostic(MathDiagnosticOperation::GeometryQuery, MathDiagnosticSource::Geometry,
                       {MathError::DegenerateInput, 0.0f, 0.0f,
                        "GJK reached the iteration limit before convergence."},
                       currentMathPolicy(), false);
  return false;
}

struct EpaPenetration3 {
  bool hit = false;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float depth = 0.0f;
};

[[nodiscard]] inline EpaPenetration3 epaPenetrationFallback(const Aabb3 lhs, const Aabb3 rhs) {
  const ContactManifold3 manifold = manifoldAabbAabb(lhs, rhs);
  return {.hit = manifold.hit, .normal = manifold.normal, .depth = manifold.penetration};
}

} // namespace aster
