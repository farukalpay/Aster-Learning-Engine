// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/math/transform.hpp"

namespace aster {

Transform Transform::fromEuler(const Vec3 position, const Vec3 rotation_xyz, const Vec3 scale) {
  return {.position = position, .rotation = quatFromEulerXyz(rotation_xyz), .scale = scale};
}

Mat4 Transform::matrix() const {
  return translation(position) * mat4FromQuat(rotation) * aster::scale(scale);
}

} // namespace aster
