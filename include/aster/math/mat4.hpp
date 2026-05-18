// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat3.hpp"
#include "aster/math/vec.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace aster {

enum class CoordinateHandedness {
  RightHanded,
  LeftHanded,
};

enum class ClipDepthRange {
  ZeroToOne,
  NegativeOneToOne,
};

enum class DepthDirection {
  ForwardZ,
  ReverseZ,
};

struct ProjectionPolicy {
  CoordinateHandedness handedness = CoordinateHandedness::RightHanded;
  ClipDepthRange depth_range = ClipDepthRange::ZeroToOne;
  DepthDirection depth_direction = DepthDirection::ReverseZ;
};

inline constexpr ProjectionPolicy defaultProjectionPolicy() {
  return {};
}

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

inline float at(const Mat4 &matrix, const int row, const int column) {
  return matrix.m[column * 4 + row];
}

inline void setAt(Mat4 &matrix, const int row, const int column, const float value) {
  matrix.m[column * 4 + row] = value;
}

inline Vec4 column(const Mat4 &matrix, const int index) {
  const int base = index * 4;
  return {matrix.m[base + 0], matrix.m[base + 1], matrix.m[base + 2], matrix.m[base + 3]};
}

inline Mat4 mat4FromColumns(const Vec4 x, const Vec4 y, const Vec4 z, const Vec4 w) {
  return {{{x.x, x.y, x.z, x.w, y.x, y.y, y.z, y.w, z.x, z.y, z.z, z.w, w.x, w.y, w.z,
            w.w}}};
}

inline Mat4 multiply(const Mat4 &lhs, const Mat4 &rhs) {
  Mat4 out{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += at(lhs, row, k) * at(rhs, k, column);
      }
      setAt(out, row, column, sum);
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
      setAt(out, row, column, at(matrix, column, row));
    }
  }
  return out;
}

inline Mat3 upperLeftMat3(const Mat4 &matrix) {
  Mat3 out{};
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      setAt(out, row, column, at(matrix, row, column));
    }
  }
  return out;
}

inline Mat4 mat4FromMat3(const Mat3 &matrix) {
  Mat4 out = identity();
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      setAt(out, row, column, at(matrix, row, column));
    }
  }
  return out;
}

inline float determinant3x3(const float a00, const float a01, const float a02, const float a10,
                            const float a11, const float a12, const float a20,
                            const float a21, const float a22) {
  return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
         a02 * (a10 * a21 - a11 * a20);
}

inline float determinant(const Mat4 &matrix) {
  float det = 0.0f;
  for (int column = 0; column < 4; ++column) {
    float minor[9]{};
    int index = 0;
    for (int c = 0; c < 4; ++c) {
      if (c == column) {
        continue;
      }
      for (int r = 1; r < 4; ++r) {
        minor[index++] = at(matrix, r, c);
      }
    }
    const float sign = (column % 2) == 0 ? 1.0f : -1.0f;
    det += sign * at(matrix, 0, column) *
           determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7],
                          minor[2], minor[5], minor[8]);
  }
  return det;
}

inline std::optional<Mat4> try_inverse(const Mat4 &matrix,
                                       const float epsilon = 0.000001f) {
  const float det = determinant(matrix);
  if (std::abs(det) <= epsilon) {
    return std::nullopt;
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
          minor[index++] = at(matrix, r, c);
        }
      }
      const float sign = ((row + column) % 2) == 0 ? 1.0f : -1.0f;
      setAt(cofactors, row, column,
            sign * determinant3x3(minor[0], minor[3], minor[6], minor[1], minor[4], minor[7],
                                  minor[2], minor[5], minor[8]));
    }
  }

  Mat4 adjugate = transpose(cofactors);
  for (float &value : adjugate.m) {
    value /= det;
  }
  return adjugate;
}

inline Mat4 inverse(const Mat4 &matrix) {
  if (const std::optional<Mat4> result = try_inverse(matrix)) {
    return *result;
  }
  throw std::runtime_error("Mat4 inverse requested for a singular matrix.");
}

inline Mat3 normalMatrix(const Mat4 &matrix) {
  return inverseTranspose(upperLeftMat3(matrix));
}

inline std::optional<Mat3> try_normalMatrix(const Mat4 &matrix) {
  const std::optional<Mat3> inv = try_inverse(upperLeftMat3(matrix));
  if (!inv) {
    return std::nullopt;
  }
  return transpose(*inv);
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

inline Mat4 axisRotation(const Vec3 axis_value, const float radians_value) {
  const Vec3 axis = normalize(axis_value);
  if (length(axis) <= 0.000001f) {
    return identity();
  }
  const float c = std::cos(radians_value);
  const float s = std::sin(radians_value);
  const Vec3 temp = axis * (1.0f - c);

  Mat4 out = identity();
  out.m[0] = c + temp.x * axis.x;
  out.m[1] = temp.x * axis.y + s * axis.z;
  out.m[2] = temp.x * axis.z - s * axis.y;
  out.m[4] = temp.y * axis.x - s * axis.z;
  out.m[5] = c + temp.y * axis.y;
  out.m[6] = temp.y * axis.z + s * axis.x;
  out.m[8] = temp.z * axis.x + s * axis.y;
  out.m[9] = temp.z * axis.y - s * axis.x;
  out.m[10] = c + temp.z * axis.z;
  return out;
}

inline Mat4 lookAtRH(const Vec3 eye, const Vec3 center, const Vec3 up) {
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

inline Mat4 lookAtLH(const Vec3 eye, const Vec3 center, const Vec3 up) {
  const Vec3 f = normalize(center - eye);
  const Vec3 s = normalize(cross(up, f));
  const Vec3 u = cross(f, s);

  Mat4 out = identity();
  out.m[0] = s.x;
  out.m[4] = s.y;
  out.m[8] = s.z;
  out.m[1] = u.x;
  out.m[5] = u.y;
  out.m[9] = u.z;
  out.m[2] = f.x;
  out.m[6] = f.y;
  out.m[10] = f.z;
  out.m[12] = -dot(s, eye);
  out.m[13] = -dot(u, eye);
  out.m[14] = -dot(f, eye);
  return out;
}

inline Mat4 lookAt(const Vec3 eye, const Vec3 center, const Vec3 up,
                   const CoordinateHandedness handedness = CoordinateHandedness::RightHanded) {
  return handedness == CoordinateHandedness::LeftHanded ? lookAtLH(eye, center, up)
                                                        : lookAtRH(eye, center, up);
}

inline Mat4 look_at(const Vec3 eye, const Vec3 center, const Vec3 up) {
  return lookAtRH(eye, center, up);
}

inline Mat4 perspectiveRH_ZO(const float vertical_fov_radians, const float aspect_ratio,
                             const float near_plane, const float far_plane) {
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = far_plane / (near_plane - far_plane);
  out.m[11] = -1.0f;
  out.m[14] = -(far_plane * near_plane) / (far_plane - near_plane);
  return out;
}

inline Mat4 perspectiveRH_ReverseZ_ZO(const float vertical_fov_radians,
                                      const float aspect_ratio, const float near_plane,
                                      const float far_plane) {
  Mat4 out{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  out.m[0] = 1.0f / (aspect_ratio * tan_half_fov);
  out.m[5] = 1.0f / tan_half_fov;
  out.m[10] = near_plane / (far_plane - near_plane);
  out.m[11] = -1.0f;
  out.m[14] = (far_plane * near_plane) / (far_plane - near_plane);
  return out;
}

inline Mat4 perspectiveRH_NO(const float vertical_fov_radians, const float aspect_ratio,
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

inline Mat4 perspective(const float vertical_fov_radians, const float aspect_ratio,
                        const float near_plane, const float far_plane,
                        const ProjectionPolicy policy = defaultProjectionPolicy()) {
  if (policy.handedness != CoordinateHandedness::RightHanded) {
    throw std::invalid_argument("Aster perspective currently supports right-handed view space.");
  }
  if (policy.depth_range == ClipDepthRange::NegativeOneToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveRH_NO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::ZeroToOne &&
      policy.depth_direction == DepthDirection::ForwardZ) {
    return perspectiveRH_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  if (policy.depth_range == ClipDepthRange::ZeroToOne &&
      policy.depth_direction == DepthDirection::ReverseZ) {
    return perspectiveRH_ReverseZ_ZO(vertical_fov_radians, aspect_ratio, near_plane, far_plane);
  }
  throw std::invalid_argument("Aster reverse-Z projection requires zero-to-one clip depth.");
}

inline Mat4 orthographicRH_ReverseZ_ZO(const float left, const float right, const float bottom,
                                       const float top, const float near_plane,
                                       const float far_plane) {
  Mat4 out = identity();
  out.m[0] = 2.0f / (right - left);
  out.m[5] = 2.0f / (top - bottom);
  out.m[10] = 1.0f / (far_plane - near_plane);
  out.m[12] = -(right + left) / (right - left);
  out.m[13] = -(top + bottom) / (top - bottom);
  out.m[14] = far_plane / (far_plane - near_plane);
  return out;
}

inline Mat4 orthographic(const float left, const float right, const float bottom, const float top,
                         const float near_plane, const float far_plane,
                         const ProjectionPolicy policy = defaultProjectionPolicy()) {
  if (policy.handedness != CoordinateHandedness::RightHanded ||
      policy.depth_range != ClipDepthRange::ZeroToOne ||
      policy.depth_direction != DepthDirection::ReverseZ) {
    throw std::invalid_argument("Aster orthographic currently supports RH reverse-Z zero-to-one.");
  }
  return orthographicRH_ReverseZ_ZO(left, right, bottom, top, near_plane, far_plane);
}

inline Vec3 project(const Vec3 point, const Mat4 &world_to_clip, const Vec2 viewport_origin,
                    const Vec2 viewport_size) {
  const Vec4 clip = world_to_clip * Vec4{point.x, point.y, point.z, 1.0f};
  const float inv_w = std::abs(clip.w) > 0.000001f ? 1.0f / clip.w : 1.0f;
  const Vec3 ndc{clip.x * inv_w, clip.y * inv_w, clip.z * inv_w};
  return {viewport_origin.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x,
          viewport_origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y, ndc.z};
}

inline Vec3 unproject(const Vec3 window, const Mat4 &clip_to_world, const Vec2 viewport_origin,
                      const Vec2 viewport_size) {
  const Vec4 clip{(window.x - viewport_origin.x) / viewport_size.x * 2.0f - 1.0f,
                  1.0f - (window.y - viewport_origin.y) / viewport_size.y * 2.0f,
                  window.z, 1.0f};
  const Vec4 world = clip_to_world * clip;
  if (std::abs(world.w) <= 0.000001f) {
    return {world.x, world.y, world.z};
  }
  return {world.x / world.w, world.y / world.w, world.z / world.w};
}

} // namespace aster
