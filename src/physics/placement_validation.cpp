// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/placement_validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr float kPlacementEpsilon = 0.000001f;

aster::Vec2 minVec2(const aster::Vec2 lhs, const aster::Vec2 rhs) {
  return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y)};
}

aster::Vec2 maxVec2(const aster::Vec2 lhs, const aster::Vec2 rhs) {
  return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y)};
}

aster::Vec3 minVec3(const aster::Vec3 lhs, const aster::Vec3 rhs) {
  return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z)};
}

aster::Vec3 maxVec3(const aster::Vec3 lhs, const aster::Vec3 rhs) {
  return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z)};
}

void requireValid(const aster::PlacementAabb box) {
  if (!aster::isValid(box)) {
    throw std::invalid_argument("Placement AABB must be valid.");
  }
}

void requireValid(const aster::PlacementFootprint footprint) {
  if (!aster::isValid(footprint)) {
    throw std::invalid_argument("Placement footprint must be valid.");
  }
}

void requireValid(const aster::PlacementEllipse ellipse) {
  if (ellipse.radius.x <= 0.0f || ellipse.radius.y <= 0.0f) {
    throw std::invalid_argument("Placement ellipse radii must be positive.");
  }
}

void requireValid(const aster::PlacementOrientedEllipse ellipse) {
  if (ellipse.radius.x <= 0.0f || ellipse.radius.y <= 0.0f) {
    throw std::invalid_argument("Placement oriented ellipse radii must be positive.");
  }
}

void requireValid(const aster::PlacementEllipseBand band) {
  requireValid(band.ellipse);
  if (band.inner_normalized_radius < 0.0f ||
      band.outer_normalized_radius < band.inner_normalized_radius) {
    throw std::invalid_argument("Placement ellipse band radii must be ordered.");
  }
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

struct OrientedEllipseFrame {
  aster::Vec2 side{};
  aster::Vec2 forward{};
};

OrientedEllipseFrame orientedEllipseFrame(const aster::PlacementOrientedEllipse ellipse) {
  aster::Vec2 forward = aster::normalize(ellipse.forward);
  if (aster::length(forward) <= kPlacementEpsilon) {
    forward = {0.0f, 1.0f};
  }
  return {{-forward.y, forward.x}, forward};
}

aster::Vec2 orientedEllipseLocalPoint(const aster::PlacementOrientedEllipse ellipse,
                                      const OrientedEllipseFrame frame, const aster::Vec2 point) {
  const aster::Vec2 offset = point - ellipse.center;
  return {aster::dot(offset, frame.side), aster::dot(offset, frame.forward)};
}

float normalizedDistanceFromOrientedLocal(const aster::PlacementOrientedEllipse ellipse,
                                          const aster::Vec2 local_point) {
  const float radius_side = directionalRadius(ellipse.radius.x, ellipse.radius_side_negative,
                                              ellipse.radius_side_positive, local_point.x);
  const float radius_forward = directionalRadius(ellipse.radius.y, ellipse.radius_forward_negative,
                                                 ellipse.radius_forward_positive, local_point.y);
  const float x = local_point.x / std::max(radius_side, kPlacementEpsilon);
  const float y = local_point.y / std::max(radius_forward, kPlacementEpsilon);
  return std::sqrt(x * x + y * y);
}

bool localSegmentIntersectsOrientedEllipse(const aster::PlacementOrientedEllipse ellipse,
                                           const aster::Vec2 a, const aster::Vec2 b) {
  const aster::Vec2 delta = b - a;
  std::vector<float> breaks{0.0f, 1.0f};
  const auto add_zero_crossing = [&](const float start, const float extent) {
    if (std::abs(extent) <= kPlacementEpsilon) {
      return;
    }
    const float t = -start / extent;
    if (t > 0.0f && t < 1.0f) {
      breaks.push_back(t);
    }
  };
  add_zero_crossing(a.x, delta.x);
  add_zero_crossing(a.y, delta.y);
  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end(),
                           [](const float lhs, const float rhs) {
                             return std::abs(lhs - rhs) <= kPlacementEpsilon;
                           }),
               breaks.end());

  for (std::size_t i = 0; i + 1u < breaks.size(); ++i) {
    const float low = breaks[i];
    const float high = breaks[i + 1u];
    const float mid = (low + high) * 0.5f;
    const aster::Vec2 midpoint = a + delta * mid;
    const float radius_side = directionalRadius(ellipse.radius.x, ellipse.radius_side_negative,
                                                ellipse.radius_side_positive, midpoint.x);
    const float radius_forward =
        directionalRadius(ellipse.radius.y, ellipse.radius_forward_negative,
                          ellipse.radius_forward_positive, midpoint.y);
    const float inv_side_sq = 1.0f / std::max(radius_side * radius_side, kPlacementEpsilon);
    const float inv_forward_sq =
        1.0f / std::max(radius_forward * radius_forward, kPlacementEpsilon);
    const float quadratic = delta.x * delta.x * inv_side_sq + delta.y * delta.y * inv_forward_sq;
    const float linear = 2.0f * (a.x * delta.x * inv_side_sq + a.y * delta.y * inv_forward_sq);
    const float best_t =
        quadratic > kPlacementEpsilon ? std::clamp(-linear / (2.0f * quadratic), low, high) : low;
    const aster::Vec2 best = a + delta * best_t;
    const float normalized_sq = best.x * best.x * inv_side_sq + best.y * best.y * inv_forward_sq;
    if (normalized_sq <= 1.0f + kPlacementEpsilon) {
      return true;
    }
  }
  return false;
}

} // namespace

namespace aster {

PlacementAabb emptyPlacementAabb() {
  return {{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
           std::numeric_limits<float>::max()},
          {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
           std::numeric_limits<float>::lowest()}};
}

PlacementAabb makePlacementAabb(const Vec3 center, const Vec3 half_extents) {
  if (half_extents.x < 0.0f || half_extents.y < 0.0f || half_extents.z < 0.0f) {
    throw std::invalid_argument("Placement AABB half extents must be non-negative.");
  }
  return {center - half_extents, center + half_extents};
}

PlacementAabb expandPlacementAabb(const PlacementAabb box, const Vec3 amount) {
  requireValid(box);
  if (amount.x < 0.0f || amount.y < 0.0f || amount.z < 0.0f) {
    throw std::invalid_argument("Placement AABB expansion must be non-negative.");
  }
  return {box.min - amount, box.max + amount};
}

void encapsulate(PlacementAabb &box, const Vec3 point) {
  if (!isValid(box)) {
    box = {point, point};
    return;
  }
  box.min = minVec3(box.min, point);
  box.max = maxVec3(box.max, point);
}

void encapsulate(PlacementAabb &box, const PlacementAabb other) {
  if (!isValid(other)) {
    return;
  }
  if (!isValid(box)) {
    box = other;
    return;
  }
  box.min = minVec3(box.min, other.min);
  box.max = maxVec3(box.max, other.max);
}

bool isValid(const PlacementAabb box) {
  return box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
}

bool contains(const PlacementAabb box, const Vec3 point) {
  return isValid(box) && point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
         point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

bool overlaps(const PlacementAabb lhs, const PlacementAabb rhs) {
  return isValid(lhs) && isValid(rhs) && lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x &&
         lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y && lhs.min.z <= rhs.max.z &&
         lhs.max.z >= rhs.min.z;
}

PlacementFootprint projectFootprint(const PlacementAabb box) {
  requireValid(box);
  return {{box.min.x, box.min.z}, {box.max.x, box.max.z}};
}

PlacementFootprint makePlacementFootprint(const Vec2 center, const Vec2 half_extents) {
  if (half_extents.x < 0.0f || half_extents.y < 0.0f) {
    throw std::invalid_argument("Placement footprint half extents must be non-negative.");
  }
  return {center - half_extents, center + half_extents};
}

PlacementFootprint makePlacementFootprintFromBounds(const Vec2 min, const Vec2 max) {
  return {minVec2(min, max), maxVec2(min, max)};
}

PlacementFootprint expandPlacementFootprint(const PlacementFootprint footprint, const Vec2 amount) {
  requireValid(footprint);
  if (amount.x < 0.0f || amount.y < 0.0f) {
    throw std::invalid_argument("Placement footprint expansion must be non-negative.");
  }
  return {footprint.min - amount, footprint.max + amount};
}

PlacementFootprint triangleFootprint(const Vec3 a, const Vec3 b, const Vec3 c) {
  PlacementFootprint footprint{{a.x, a.z}, {a.x, a.z}};
  footprint.min = minVec2(footprint.min, {b.x, b.z});
  footprint.min = minVec2(footprint.min, {c.x, c.z});
  footprint.max = maxVec2(footprint.max, {b.x, b.z});
  footprint.max = maxVec2(footprint.max, {c.x, c.z});
  return footprint;
}

bool isValid(const PlacementFootprint footprint) {
  return footprint.min.x <= footprint.max.x && footprint.min.y <= footprint.max.y;
}

bool contains(const PlacementFootprint footprint, const Vec2 point) {
  return isValid(footprint) && point.x >= footprint.min.x && point.x <= footprint.max.x &&
         point.y >= footprint.min.y && point.y <= footprint.max.y;
}

bool overlaps(const PlacementFootprint lhs, const PlacementFootprint rhs) {
  return isValid(lhs) && isValid(rhs) && lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x &&
         lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y;
}

float normalizedDistance(const PlacementEllipse ellipse, const Vec2 point) {
  requireValid(ellipse);
  const float dx = (point.x - ellipse.center.x) / ellipse.radius.x;
  const float dz = (point.y - ellipse.center.y) / ellipse.radius.y;
  return std::sqrt(dx * dx + dz * dz);
}

bool contains(const PlacementEllipse ellipse, const Vec2 point) {
  return normalizedDistance(ellipse, point) <= 1.0f;
}

bool contains(const PlacementEllipse ellipse, const PlacementFootprint footprint) {
  requireValid(ellipse);
  requireValid(footprint);
  return contains(ellipse, footprint.min) &&
         contains(ellipse, Vec2{footprint.max.x, footprint.min.y}) &&
         contains(ellipse, footprint.max) &&
         contains(ellipse, Vec2{footprint.min.x, footprint.max.y});
}

bool intersects(const PlacementEllipse ellipse, const PlacementFootprint footprint) {
  requireValid(ellipse);
  requireValid(footprint);
  const Vec2 closest{clamp(ellipse.center.x, footprint.min.x, footprint.max.x),
                     clamp(ellipse.center.y, footprint.min.y, footprint.max.y)};
  return contains(ellipse, closest);
}

float normalizedDistance(const PlacementOrientedEllipse ellipse, const Vec2 point) {
  requireValid(ellipse);
  const OrientedEllipseFrame frame = orientedEllipseFrame(ellipse);
  return normalizedDistanceFromOrientedLocal(ellipse,
                                             orientedEllipseLocalPoint(ellipse, frame, point));
}

bool contains(const PlacementOrientedEllipse ellipse, const Vec2 point) {
  return normalizedDistance(ellipse, point) <= 1.0f;
}

bool intersects(const PlacementOrientedEllipse ellipse, const PlacementFootprint footprint) {
  requireValid(ellipse);
  requireValid(footprint);
  const OrientedEllipseFrame frame = orientedEllipseFrame(ellipse);
  const Vec2 corners[4] = {
      footprint.min,
      {footprint.max.x, footprint.min.y},
      footprint.max,
      {footprint.min.x, footprint.max.y},
  };
  for (const Vec2 corner : corners) {
    if (contains(ellipse, corner)) {
      return true;
    }
  }
  if (contains(footprint, ellipse.center)) {
    return true;
  }
  for (int i = 0; i < 4; ++i) {
    const Vec2 a = orientedEllipseLocalPoint(ellipse, frame, corners[i]);
    const Vec2 b = orientedEllipseLocalPoint(ellipse, frame, corners[(i + 1) % 4]);
    if (localSegmentIntersectsOrientedEllipse(ellipse, a, b)) {
      return true;
    }
  }
  return false;
}

bool contains(const PlacementEllipseBand band, const Vec2 point) {
  requireValid(band);
  const float distance = normalizedDistance(band.ellipse, point);
  return distance >= band.inner_normalized_radius && distance <= band.outer_normalized_radius;
}

void PlacementValidator::clear() {
  forbidden_aabbs_.clear();
  forbidden_footprints_.clear();
  forbidden_ellipses_.clear();
  forbidden_oriented_ellipses_.clear();
}

void PlacementValidator::addForbiddenAabb(const PlacementAabb volume) {
  requireValid(volume);
  forbidden_aabbs_.push_back(volume);
}

void PlacementValidator::addForbiddenFootprint(const PlacementFootprint footprint) {
  requireValid(footprint);
  forbidden_footprints_.push_back(footprint);
}

void PlacementValidator::addForbiddenEllipse(const PlacementEllipse ellipse) {
  requireValid(ellipse);
  forbidden_ellipses_.push_back(ellipse);
}

void PlacementValidator::addForbiddenOrientedEllipse(const PlacementOrientedEllipse ellipse) {
  requireValid(ellipse);
  forbidden_oriented_ellipses_.push_back(ellipse);
}

bool PlacementValidator::rejectsPoint(const Vec3 point) const {
  const Vec2 footprint_point{point.x, point.z};
  for (const PlacementAabb &volume : forbidden_aabbs_) {
    if (contains(volume, point)) {
      return true;
    }
  }
  for (const PlacementFootprint &footprint : forbidden_footprints_) {
    if (contains(footprint, footprint_point)) {
      return true;
    }
  }
  for (const PlacementEllipse &ellipse : forbidden_ellipses_) {
    if (contains(ellipse, footprint_point)) {
      return true;
    }
  }
  for (const PlacementOrientedEllipse &ellipse : forbidden_oriented_ellipses_) {
    if (contains(ellipse, footprint_point)) {
      return true;
    }
  }
  return false;
}

bool PlacementValidator::rejectsAabb(const PlacementAabb volume) const {
  requireValid(volume);
  for (const PlacementAabb &forbidden : forbidden_aabbs_) {
    if (overlaps(forbidden, volume)) {
      return true;
    }
  }
  const PlacementFootprint footprint = projectFootprint(volume);
  for (const PlacementFootprint &forbidden : forbidden_footprints_) {
    if (overlaps(forbidden, footprint)) {
      return true;
    }
  }
  for (const PlacementEllipse &ellipse : forbidden_ellipses_) {
    if (intersects(ellipse, footprint)) {
      return true;
    }
  }
  for (const PlacementOrientedEllipse &ellipse : forbidden_oriented_ellipses_) {
    if (intersects(ellipse, footprint)) {
      return true;
    }
  }
  return false;
}

bool PlacementValidator::rejectsFootprint(const PlacementFootprint footprint) const {
  requireValid(footprint);
  for (const PlacementAabb &volume : forbidden_aabbs_) {
    if (overlaps(projectFootprint(volume), footprint)) {
      return true;
    }
  }
  for (const PlacementFootprint &forbidden : forbidden_footprints_) {
    if (overlaps(forbidden, footprint)) {
      return true;
    }
  }
  for (const PlacementEllipse &ellipse : forbidden_ellipses_) {
    if (intersects(ellipse, footprint)) {
      return true;
    }
  }
  for (const PlacementOrientedEllipse &ellipse : forbidden_oriented_ellipses_) {
    if (intersects(ellipse, footprint)) {
      return true;
    }
  }
  return false;
}

bool PlacementValidator::rejectsTriangleFootprint(const Vec3 a, const Vec3 b, const Vec3 c) const {
  return rejectsFootprint(triangleFootprint(a, b, c));
}

bool PlacementValidator::allowsPoint(const Vec3 point) const {
  return !rejectsPoint(point);
}

bool PlacementValidator::allowsAabb(const PlacementAabb volume) const {
  return !rejectsAabb(volume);
}

bool PlacementValidator::allowsFootprint(const PlacementFootprint footprint) const {
  return !rejectsFootprint(footprint);
}

bool PlacementValidator::allowsTriangleFootprint(const Vec3 a, const Vec3 b, const Vec3 c) const {
  return !rejectsTriangleFootprint(a, b, c);
}

const std::vector<PlacementAabb> &PlacementValidator::forbiddenAabbs() const {
  return forbidden_aabbs_;
}

const std::vector<PlacementFootprint> &PlacementValidator::forbiddenFootprints() const {
  return forbidden_footprints_;
}

const std::vector<PlacementEllipse> &PlacementValidator::forbiddenEllipses() const {
  return forbidden_ellipses_;
}

const std::vector<PlacementOrientedEllipse> &PlacementValidator::forbiddenOrientedEllipses() const {
  return forbidden_oriented_ellipses_;
}

} // namespace aster
