// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

enum class PredicateSign {
  Negative = -1,
  Zero = 0,
  Positive = 1,
  Uncertain = 2,
};

[[nodiscard]] inline PredicateSign signWithTolerance(const double value,
                                                     const double tolerance = 1.0e-12) {
  if (std::abs(value) <= tolerance) {
    return PredicateSign::Zero;
  }
  return value > 0.0 ? PredicateSign::Positive : PredicateSign::Negative;
}

[[nodiscard]] inline double orient2dValue(const Vec2 a, const Vec2 b, const Vec2 c) {
  return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
         (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

[[nodiscard]] inline PredicateSign orient2d(const Vec2 a, const Vec2 b, const Vec2 c) {
  const double value = orient2dValue(a, b, c);
  const double scale = std::max({std::abs(a.x), std::abs(a.y), std::abs(b.x), std::abs(b.y),
                                 std::abs(c.x), std::abs(c.y), 1.0f});
  const PredicateSign sign = signWithTolerance(value, scale * scale * 1.0e-12);
  if (sign == PredicateSign::Zero && value != 0.0) {
    recordMathDiagnostic(MathDiagnosticOperation::RobustPredicate,
                         MathDiagnosticSource::Geometry,
                         {MathError::DegenerateInput, 0.0f, static_cast<float>(std::abs(value)),
                          "orient2d result is near the robustness threshold."},
                         currentMathPolicy(), false);
  }
  return sign;
}

[[nodiscard]] inline double orient3dValue(const Vec3 a, const Vec3 b, const Vec3 c, const Vec3 d) {
  const DVec3 ad{static_cast<double>(a.x) - d.x, static_cast<double>(a.y) - d.y,
                 static_cast<double>(a.z) - d.z};
  const DVec3 bd{static_cast<double>(b.x) - d.x, static_cast<double>(b.y) - d.y,
                 static_cast<double>(b.z) - d.z};
  const DVec3 cd{static_cast<double>(c.x) - d.x, static_cast<double>(c.y) - d.y,
                 static_cast<double>(c.z) - d.z};
  return dot(ad, cross(bd, cd));
}

[[nodiscard]] inline PredicateSign orient3d(const Vec3 a, const Vec3 b, const Vec3 c,
                                            const Vec3 d) {
  return signWithTolerance(orient3dValue(a, b, c, d), 1.0e-10);
}

[[nodiscard]] inline double incircleValue(const Vec2 a, const Vec2 b, const Vec2 c,
                                          const Vec2 d) {
  const double ax = static_cast<double>(a.x) - d.x;
  const double ay = static_cast<double>(a.y) - d.y;
  const double bx = static_cast<double>(b.x) - d.x;
  const double by = static_cast<double>(b.y) - d.y;
  const double cx = static_cast<double>(c.x) - d.x;
  const double cy = static_cast<double>(c.y) - d.y;
  return (ax * ax + ay * ay) * (bx * cy - by * cx) -
         (bx * bx + by * by) * (ax * cy - ay * cx) +
         (cx * cx + cy * cy) * (ax * by - ay * bx);
}

[[nodiscard]] inline PredicateSign incircle(const Vec2 a, const Vec2 b, const Vec2 c,
                                            const Vec2 d) {
  return signWithTolerance(incircleValue(a, b, c, d), 1.0e-9);
}

} // namespace aster
