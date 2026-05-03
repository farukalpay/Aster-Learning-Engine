// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/contact_query.hpp"

#include <algorithm>

namespace {

float segmentDistance(const float lhs_center, const float lhs_half_segment, const float rhs_center,
                      const float rhs_half_segment) {
  const float lhs_min = lhs_center - std::max(lhs_half_segment, 0.0f);
  const float lhs_max = lhs_center + std::max(lhs_half_segment, 0.0f);
  const float rhs_min = rhs_center - std::max(rhs_half_segment, 0.0f);
  const float rhs_max = rhs_center + std::max(rhs_half_segment, 0.0f);
  if (lhs_max < rhs_min) {
    return rhs_min - lhs_max;
  }
  if (rhs_max < lhs_min) {
    return lhs_min - rhs_max;
  }
  return 0.0f;
}

} // namespace

namespace aster {

bool overlaps(const VerticalCapsuleContactVolume &lhs, const VerticalCapsuleContactVolume &rhs) {
  const float combined_radius = std::max(lhs.radius, 0.0f) + std::max(rhs.radius, 0.0f);
  const Vec2 planar_delta{lhs.center.x - rhs.center.x, lhs.center.z - rhs.center.z};
  if (dot(planar_delta, planar_delta) > combined_radius * combined_radius) {
    return false;
  }

  const float vertical_gap =
      segmentDistance(lhs.center.y, lhs.half_segment, rhs.center.y, rhs.half_segment);
  return vertical_gap <= combined_radius;
}

} // namespace aster
