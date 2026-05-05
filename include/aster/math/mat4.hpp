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

inline Vec4 operator*(const Mat4 &matrix, const Vec4 value) {
  return {
      matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z +
          matrix.m[12] * value.w,
      matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z +
          matrix.m[13] * value.w,
      matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z +
          matrix.m[14] * value.w,
      matrix.m[3] * value.x + matrix.m[7] * value.y + matrix.m[11] * value.z +
          matrix.m[15] * value.w,
  };
}

inline Vec3 transformPoint(const Mat4 &matrix, const Vec3 point) {
  const Vec4 out = matrix * Vec4{point.x, point.y, point.z, 1.0f};
  if (std::abs(out.w) <= 0.000001f) {
    return {out.x, out.y, out.z};
  }
  return {out.x / out.w, out.y / out.w, out.z / out.w};
}

inline Vec3 transformVector(const Mat4 &matrix, const Vec3 value) {
  const Vec4 out = matrix * Vec4{value.x, value.y, value.z, 0.0f};
  return {out.x, out.y, out.z};
}

inline Mat4 transpose(const Mat4 &matrix) {
  Mat4 out{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      out.m[column * 4 + row] = matrix.m[row * 4 + column];
    }
  }
  return out;
}

inline float determinant3x3(const float a00, const float a01, const float a02, const float a10,
                            const float a11, const float a12, const float a20, const float a21,
                            const float a22) {
  return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
         a02 * (a10 * a21 - a11 * a20);
}

inline float determinant(const Mat4 &matrix) {
  const auto at = [&](const int row, const int column) {
    return matrix.m[column * 4 + row];
  };
  float det = 0.0f;
  for (int column = 0; column < 4; ++column) {
    float minor[9]{};
    int index = 0;
    for (int c = 0; c < 4; ++c) {
      if (c == column) {
        continue;
      }
      for (int r = 1; r < 4; ++r) {
        minor[index++] = at(r, c);
      }
    }
    const float sign = (column % 2) == 0 ? 1.0f : -1.0f;
    det += sign * at(0, column) *
           determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7], minor[2],
                          minor[5], minor[8]);
  }
  return det;
}

inline Mat4 inverse(const Mat4 &matrix) {
  const auto at = [&](const int row, const int column) {
    return matrix.m[column * 4 + row];
  };
  const float det = determinant(matrix);
  if (std::abs(det) <= 0.000001f) {
    return identity();
  }

  Mat4 cofactors{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      float minor[9]{};
      int index = 0;
      for (int c = 0; c < 4; ++c) {
        if (c == column) {
          continue;
        }
        for (int r = 0; r < 4; ++r) {
          if (r == row) {
            continue;
          }
          minor[index++] = at(r, c);
        }
      }
      const float sign = ((row + column) % 2) == 0 ? 1.0f : -1.0f;
      cofactors.m[column * 4 + row] =
          sign * determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7],
                                minor[2], minor[5], minor[8]);
    }
  }

  Mat4 adjugate = transpose(cofactors);
  for (float &value : adjugate.m) {
    value /= det;
  }
  return adjugate;
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
