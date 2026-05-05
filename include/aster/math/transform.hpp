// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"
#include "aster/math/vec.hpp"

namespace aster {

struct Transform {
  Vec3 position{0.0f, 0.0f, 0.0f};
  Vec3 rotation{0.0f, 0.0f, 0.0f};
  Vec3 scale{1.0f, 1.0f, 1.0f};

  [[nodiscard]] Mat4 matrix() const;
};

inline Vec3 transformPoint(const Transform &transform, const Vec3 point) {
  return aster::transformPoint(transform.matrix(), point);
}

inline Vec3 transformVector(const Transform &transform, const Vec3 value) {
  return aster::transformVector(transform.matrix(), value);
}

} // namespace aster
