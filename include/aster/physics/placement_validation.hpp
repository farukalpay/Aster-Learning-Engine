// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <vector>

namespace aster {

struct PlacementAabb {
  Vec3 min{};
  Vec3 max{};
};

struct PlacementFootprint {
  Vec2 min{};
  Vec2 max{};
};

struct PlacementEllipse {
  Vec2 center{};
  Vec2 radius{1.0f, 1.0f};
};

struct PlacementOrientedEllipse {
  Vec2 center{};
  Vec2 forward{0.0f, 1.0f};
  Vec2 radius{1.0f, 1.0f};
  float radius_side_negative = 0.0f;
  float radius_side_positive = 0.0f;
  float radius_forward_negative = 0.0f;
  float radius_forward_positive = 0.0f;
};

struct PlacementEllipseBand {
  PlacementEllipse ellipse{};
  float inner_normalized_radius = 0.0f;
  float outer_normalized_radius = 1.0f;
};

[[nodiscard]] PlacementAabb emptyPlacementAabb();
[[nodiscard]] PlacementAabb makePlacementAabb(Vec3 center, Vec3 half_extents);
[[nodiscard]] PlacementAabb expandPlacementAabb(PlacementAabb box, Vec3 amount);
void encapsulate(PlacementAabb &box, Vec3 point);
void encapsulate(PlacementAabb &box, PlacementAabb other);
[[nodiscard]] bool isValid(PlacementAabb box);
[[nodiscard]] bool contains(PlacementAabb box, Vec3 point);
[[nodiscard]] bool overlaps(PlacementAabb lhs, PlacementAabb rhs);
[[nodiscard]] PlacementFootprint projectFootprint(PlacementAabb box);

[[nodiscard]] PlacementFootprint makePlacementFootprint(Vec2 center, Vec2 half_extents);
[[nodiscard]] PlacementFootprint makePlacementFootprintFromBounds(Vec2 min, Vec2 max);
[[nodiscard]] PlacementFootprint expandPlacementFootprint(PlacementFootprint footprint,
                                                          Vec2 amount);
[[nodiscard]] PlacementFootprint triangleFootprint(Vec3 a, Vec3 b, Vec3 c);
[[nodiscard]] bool isValid(PlacementFootprint footprint);
[[nodiscard]] bool contains(PlacementFootprint footprint, Vec2 point);
[[nodiscard]] bool overlaps(PlacementFootprint lhs, PlacementFootprint rhs);
[[nodiscard]] float normalizedDistance(PlacementEllipse ellipse, Vec2 point);
[[nodiscard]] bool contains(PlacementEllipse ellipse, Vec2 point);
[[nodiscard]] bool contains(PlacementEllipse ellipse, PlacementFootprint footprint);
[[nodiscard]] bool intersects(PlacementEllipse ellipse, PlacementFootprint footprint);
[[nodiscard]] float normalizedDistance(PlacementOrientedEllipse ellipse, Vec2 point);
[[nodiscard]] bool contains(PlacementOrientedEllipse ellipse, Vec2 point);
[[nodiscard]] bool intersects(PlacementOrientedEllipse ellipse, PlacementFootprint footprint);
[[nodiscard]] bool contains(PlacementEllipseBand band, Vec2 point);

class PlacementValidator {
public:
  void clear();
  void addForbiddenAabb(PlacementAabb volume);
  void addForbiddenFootprint(PlacementFootprint footprint);
  void addForbiddenEllipse(PlacementEllipse ellipse);
  void addForbiddenOrientedEllipse(PlacementOrientedEllipse ellipse);

  [[nodiscard]] bool rejectsPoint(Vec3 point) const;
  [[nodiscard]] bool rejectsAabb(PlacementAabb volume) const;
  [[nodiscard]] bool rejectsFootprint(PlacementFootprint footprint) const;
  [[nodiscard]] bool rejectsTriangleFootprint(Vec3 a, Vec3 b, Vec3 c) const;

  [[nodiscard]] bool allowsPoint(Vec3 point) const;
  [[nodiscard]] bool allowsAabb(PlacementAabb volume) const;
  [[nodiscard]] bool allowsFootprint(PlacementFootprint footprint) const;
  [[nodiscard]] bool allowsTriangleFootprint(Vec3 a, Vec3 b, Vec3 c) const;

  [[nodiscard]] const std::vector<PlacementAabb> &forbiddenAabbs() const;
  [[nodiscard]] const std::vector<PlacementFootprint> &forbiddenFootprints() const;
  [[nodiscard]] const std::vector<PlacementEllipse> &forbiddenEllipses() const;
  [[nodiscard]] const std::vector<PlacementOrientedEllipse> &forbiddenOrientedEllipses() const;

private:
  std::vector<PlacementAabb> forbidden_aabbs_;
  std::vector<PlacementFootprint> forbidden_footprints_;
  std::vector<PlacementEllipse> forbidden_ellipses_;
  std::vector<PlacementOrientedEllipse> forbidden_oriented_ellipses_;
};

} // namespace aster
