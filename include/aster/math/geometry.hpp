// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

struct Barycentric3 {
  float u = 0.0f;
  float v = 0.0f;
  float w = 0.0f;
};

struct RayHit3 {
  bool hit = false;
  float distance = 0.0f;
  Vec3 point{};
  Vec3 normal{0.0f, 1.0f, 0.0f};
  Barycentric3 barycentric{};
};

[[nodiscard]] inline Vec3 closestPointOnSegment(const Vec3 point, const Vec3 a, const Vec3 b) {
  const Vec3 ab = b - a;
  const float len_sq = dot(ab, ab);
  if (len_sq <= 0.000001f) {
    return a;
  }
  const float t = clamp(dot(point - a, ab) / len_sq, 0.0f, 1.0f);
  return a + ab * t;
}

[[nodiscard]] inline Barycentric3 barycentricCoordinates(const Vec3 point, const Vec3 a,
                                                         const Vec3 b, const Vec3 c) {
  const Vec3 v0 = b - a;
  const Vec3 v1 = c - a;
  const Vec3 v2 = point - a;
  const float d00 = dot(v0, v0);
  const float d01 = dot(v0, v1);
  const float d11 = dot(v1, v1);
  const float d20 = dot(v2, v0);
  const float d21 = dot(v2, v1);
  const float denom = d00 * d11 - d01 * d01;
  if (std::abs(denom) <= 0.000001f) {
    return {};
  }
  const float v = (d11 * d20 - d01 * d21) / denom;
  const float w = (d00 * d21 - d01 * d20) / denom;
  return {1.0f - v - w, v, w};
}

[[nodiscard]] inline Vec3 closestPointOnTriangle(const Vec3 point, const Vec3 a, const Vec3 b,
                                                 const Vec3 c) {
  const Vec3 ab = b - a;
  const Vec3 ac = c - a;
  const Vec3 ap = point - a;
  const float d1 = dot(ab, ap);
  const float d2 = dot(ac, ap);
  if (d1 <= 0.0f && d2 <= 0.0f) {
    return a;
  }

  const Vec3 bp = point - b;
  const float d3 = dot(ab, bp);
  const float d4 = dot(ac, bp);
  if (d3 >= 0.0f && d4 <= d3) {
    return b;
  }

  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
    const float v = d1 / (d1 - d3);
    return a + ab * v;
  }

  const Vec3 cp = point - c;
  const float d5 = dot(ab, cp);
  const float d6 = dot(ac, cp);
  if (d6 >= 0.0f && d5 <= d6) {
    return c;
  }

  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
    const float w = d2 / (d2 - d6);
    return a + ac * w;
  }

  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
    const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return b + (c - b) * w;
  }

  const float denom = 1.0f / (va + vb + vc);
  const float v = vb * denom;
  const float w = vc * denom;
  return a + ab * v + ac * w;
}

[[nodiscard]] inline MathResult<float> intersectRayPlaneDistance(const Vec3 origin,
                                                                 const Vec3 direction,
                                                                 const Plane3 plane) {
  const float denom = dot(direction, plane.normal);
  if (std::abs(denom) <= 0.000001f) {
    return MathResult<float>::failure(MathError::DegenerateInput,
                                      "Ray is parallel to the plane.");
  }
  const float t = -(dot(origin, plane.normal) + plane.distance) / denom;
  if (t < 0.0f) {
    return MathResult<float>::failure(MathError::DegenerateInput,
                                      "Plane intersection is behind the ray.");
  }
  return MathResult<float>::success(t);
}

[[nodiscard]] inline RayHit3 intersectRayPlane(const Vec3 origin, const Vec3 direction,
                                               const Plane3 plane) {
  const MathResult<float> t = intersectRayPlaneDistance(origin, direction, plane);
  if (!t) {
    return {};
  }
  return {.hit = true, .distance = t.value, .point = origin + direction * t.value,
          .normal = plane.normal};
}

[[nodiscard]] inline RayHit3 intersectRayTriangle(const Vec3 origin, const Vec3 direction,
                                                  const Vec3 a, const Vec3 b, const Vec3 c,
                                                  const bool cull_backfaces = false) {
  const Vec3 edge1 = b - a;
  const Vec3 edge2 = c - a;
  const Vec3 pvec = cross(direction, edge2);
  const float det = dot(edge1, pvec);
  if (cull_backfaces) {
    if (det <= 0.000001f) {
      return {};
    }
  } else if (std::abs(det) <= 0.000001f) {
    return {};
  }
  const float inv_det = 1.0f / det;
  const Vec3 tvec = origin - a;
  const float u = dot(tvec, pvec) * inv_det;
  if (u < 0.0f || u > 1.0f) {
    return {};
  }
  const Vec3 qvec = cross(tvec, edge1);
  const float v = dot(direction, qvec) * inv_det;
  if (v < 0.0f || u + v > 1.0f) {
    return {};
  }
  const float t = dot(edge2, qvec) * inv_det;
  if (t < 0.0f) {
    return {};
  }
  const Vec3 normal = normalizeOr(cross(edge1, edge2), {0.0f, 1.0f, 0.0f});
  return {.hit = true,
          .distance = t,
          .point = origin + direction * t,
          .normal = normal,
          .barycentric = {1.0f - u - v, u, v}};
}

[[nodiscard]] inline RayHit3 intersectRaySphere(const Vec3 origin, const Vec3 direction,
                                                const Sphere3 sphere) {
  const Vec3 m = origin - sphere.center;
  const float b = dot(m, direction);
  const float c = dot(m, m) - sphere.radius * sphere.radius;
  if (c > 0.0f && b > 0.0f) {
    return {};
  }
  const float discr = b * b - c;
  if (discr < 0.0f) {
    return {};
  }
  const float t = std::max(0.0f, -b - std::sqrt(discr));
  const Vec3 point = origin + direction * t;
  return {.hit = true,
          .distance = t,
          .point = point,
          .normal = normalizeOr(point - sphere.center, {0.0f, 1.0f, 0.0f})};
}

[[nodiscard]] inline bool overlaps(const Aabb3 lhs, const Aabb3 rhs) {
  return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y &&
         lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

[[nodiscard]] inline Vec3 closestPoint(const Aabb3 box, const Vec3 point) {
  return {clamp(point.x, box.min.x, box.max.x), clamp(point.y, box.min.y, box.max.y),
          clamp(point.z, box.min.z, box.max.z)};
}

} // namespace aster
