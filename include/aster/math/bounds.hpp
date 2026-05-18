// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/geometry.hpp"
#include "aster/math/quat.hpp"

#include <array>
#include <cstddef>

namespace aster {

struct Obb3 {
  Vec3 center{};
  std::array<Vec3, 3> axis{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};
};

struct Frustum3 {
  std::array<Plane3, 6> planes{};
};

template <std::size_t PlaneCount> struct KDop3 {
  std::array<Plane3, PlaneCount> planes{};
};

[[nodiscard]] inline Obb3 obbFromTransform(const Vec3 center, const Quat rotation,
                                           const Vec3 half_extents) {
  return {center,
          {normalizeOr(rotate(rotation, {1.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f}),
           normalizeOr(rotate(rotation, {0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f}),
           normalizeOr(rotate(rotation, {0.0f, 0.0f, 1.0f}), {0.0f, 0.0f, 1.0f})},
          half_extents};
}

[[nodiscard]] inline float projectedRadius(const Obb3 box, const Vec3 axis) {
  return std::abs(dot(axis, box.axis[0])) * box.half_extents.x +
         std::abs(dot(axis, box.axis[1])) * box.half_extents.y +
         std::abs(dot(axis, box.axis[2])) * box.half_extents.z;
}

[[nodiscard]] inline bool overlaps(const Plane3 plane, const Sphere3 sphere) {
  return signedDistance(plane, sphere.center) <= sphere.radius;
}

[[nodiscard]] inline bool contains(const Frustum3 &frustum, const Vec3 point) {
  for (const Plane3 plane : frustum.planes) {
    if (signedDistance(plane, point) < 0.0f) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool overlaps(const Frustum3 &frustum, const Sphere3 sphere) {
  for (const Plane3 plane : frustum.planes) {
    if (signedDistance(plane, sphere.center) < -sphere.radius) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool overlaps(const Frustum3 &frustum, const Aabb3 box) {
  for (const Plane3 plane : frustum.planes) {
    const Vec3 positive{plane.normal.x >= 0.0f ? box.max.x : box.min.x,
                        plane.normal.y >= 0.0f ? box.max.y : box.min.y,
                        plane.normal.z >= 0.0f ? box.max.z : box.min.z};
    if (signedDistance(plane, positive) < 0.0f) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool overlaps(const Obb3 lhs, const Obb3 rhs) {
  std::array<Vec3, 15> axes{};
  std::size_t count = 0u;
  for (const Vec3 axis : lhs.axis) {
    axes[count++] = axis;
  }
  for (const Vec3 axis : rhs.axis) {
    axes[count++] = axis;
  }
  for (const Vec3 a : lhs.axis) {
    for (const Vec3 b : rhs.axis) {
      const Vec3 cross_axis = cross(a, b);
      if (lengthSquared(cross_axis) > 0.000001f) {
        axes[count++] = normalize(cross_axis);
      }
    }
  }
  const Vec3 delta = rhs.center - lhs.center;
  for (std::size_t i = 0; i < count; ++i) {
    const Vec3 axis = axes[i];
    const float distance_on_axis = std::abs(dot(delta, axis));
    if (distance_on_axis > projectedRadius(lhs, axis) + projectedRadius(rhs, axis)) {
      return false;
    }
  }
  return true;
}

template <std::size_t PlaneCount>
[[nodiscard]] inline bool contains(const KDop3<PlaneCount> &volume, const Vec3 point) {
  for (const Plane3 plane : volume.planes) {
    if (signedDistance(plane, point) < 0.0f) {
      return false;
    }
  }
  return true;
}

} // namespace aster
