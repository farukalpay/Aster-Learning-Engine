// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <array>
#include <cmath>

namespace aster {

struct Mat4 {
  std::array<float, 16> m{};

  [[nodiscard]] const float *data() const {
    return m.data();
  }

  [[nodiscard]] float *data() {
    return m.data();
  }
};

inline Mat4 identity() {
  Mat4 out{};
  out.m[0] = 1.0f;
  out.m[5] = 1.0f;
  out.m[10] = 1.0f;
  out.m[15] = 1.0f;
  return out;
}

inline Mat4 multiply(const Mat4 &lhs, const Mat4 &rhs) {
  Mat4 out{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += lhs.m[k * 4 + row] * rhs.m[column * 4 + k];
      }
      out.m[column * 4 + row] = sum;
    }
  }
  return out;
}

inline Mat4 operator*(const Mat4 &lhs, const Mat4 &rhs) {
  return multiply(lhs, rhs);
}

inline Mat4 translation(const Vec3 offset) {
  Mat4 out = identity();
  out.m[12] = offset.x;
  out.m[13] = offset.y;
  out.m[14] = offset.z;
  return out;
}

inline Mat4 scale(const Vec3 value) {
  Mat4 out = identity();
  out.m[0] = value.x;
  out.m[5] = value.y;
  out.m[10] = value.z;
  return out;
}

inline Mat4 rotation_x(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[5] = c;
  out.m[6] = s;
  out.m[9] = -s;
  out.m[10] = c;
  return out;
}

inline Mat4 rotation_y(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[0] = c;
  out.m[2] = -s;
  out.m[8] = s;
  out.m[10] = c;
  return out;
}

inline Mat4 rotation_z(const float radians_value) {
  Mat4 out = identity();
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  out.m[0] = c;
  out.m[1] = s;
  out.m[4] = -s;
  out.m[5] = c;
  return out;
}

inline Mat4 perspective(const float vertical_fov_radians, const float aspect_ratio,
                        const float near_plane, const float far_plane) {
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = -(far_plane + near_plane) / (far_plane - near_plane);
  out.m[11] = -1.0f;
  out.m[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
  return out;
}

inline Mat4 look_at(const Vec3 eye, const Vec3 center, const Vec3 up) {
  const Vec3 f = normalize(center - eye);
  const Vec3 s = normalize(cross(f, up));
  const Vec3 u = cross(s, f);

  Mat4 out = identity();
  out.m[0] = s.x;
  out.m[4] = s.y;
  out.m[8] = s.z;
  out.m[1] = u.x;
  out.m[5] = u.y;
  out.m[9] = u.z;
  out.m[2] = -f.x;
  out.m[6] = -f.y;
  out.m[10] = -f.z;
  out.m[12] = -dot(s, eye);
  out.m[13] = -dot(u, eye);
  out.m[14] = dot(f, eye);
  return out;
}

} // namespace aster
