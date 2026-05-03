// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cmath>

namespace aster {

inline Vec3 aces_tonemap(const Vec3 value) {
  constexpr float a = 2.51f;
  constexpr float b = 0.03f;
  constexpr float c = 2.43f;
  constexpr float d = 0.59f;
  constexpr float e = 0.14f;
  return clamp({(value.x * (a * value.x + b)) / (value.x * (c * value.x + d) + e),
                (value.y * (a * value.y + b)) / (value.y * (c * value.y + d) + e),
                (value.z * (a * value.z + b)) / (value.z * (c * value.z + d) + e)},
               0.0f, 1.0f);
}

inline Vec3 reinhard_tonemap(const Vec3 value) {
  return {
      value.x / (value.x + 1.0f),
      value.y / (value.y + 1.0f),
      value.z / (value.z + 1.0f),
  };
}

inline Vec3 gamma_encode(const Vec3 linear) {
  constexpr float inverse_gamma = 1.0f / 2.2f;
  return {
      std::pow(clamp(linear.x, 0.0f, 1.0f), inverse_gamma),
      std::pow(clamp(linear.y, 0.0f, 1.0f), inverse_gamma),
      std::pow(clamp(linear.z, 0.0f, 1.0f), inverse_gamma),
  };
}

} // namespace aster
