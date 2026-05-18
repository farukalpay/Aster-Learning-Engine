// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"
#include "aster/math/vec.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

struct Quat {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;

  constexpr Quat() = default;
  constexpr Quat(const float x_value, const float y_value, const float z_value,
                 const float w_value)
      : x(x_value), y(y_value), z(z_value), w(w_value) {}
};

inline Quat identityQuat() {
  return {};
}

inline float dot(const Quat lhs, const Quat rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

inline float length(const Quat value) {
  return std::sqrt(dot(value, value));
}

inline Quat normalize(const Quat value) {
  const float len = length(value);
  if (len <= 0.000001f) {
    return identityQuat();
  }
  return {value.x / len, value.y / len, value.z / len, value.w / len};
}

inline Quat conjugate(const Quat value) {
  return {-value.x, -value.y, -value.z, value.w};
}

inline Quat inverse(const Quat value) {
  const float len_sq = dot(value, value);
  if (len_sq <= 0.000001f) {
    return identityQuat();
  }
  const Quat c = conjugate(value);
  return {c.x / len_sq, c.y / len_sq, c.z / len_sq, c.w / len_sq};
}

inline Quat operator-(const Quat value) {
  return {-value.x, -value.y, -value.z, -value.w};
}

inline Quat operator*(const Quat lhs, const float scalar) {
  return {lhs.x * scalar, lhs.y * scalar, lhs.z * scalar, lhs.w * scalar};
}

inline Quat operator+(const Quat lhs, const Quat rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

inline Quat operator*(const Quat lhs, const Quat rhs) {
  return {
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

inline Quat axisAngle(Vec3 axis, const float radians_value) {
  axis = normalize(axis);
  if (length(axis) <= 0.000001f) {
    return identityQuat();
  }
  const float half_angle = radians_value * 0.5f;
  const float s = std::sin(half_angle);
  return normalize(Quat{axis.x * s, axis.y * s, axis.z * s, std::cos(half_angle)});
}

inline Quat quatFromEulerXyz(const Vec3 euler) {
  const Quat qx = axisAngle({1.0f, 0.0f, 0.0f}, euler.x);
  const Quat qy = axisAngle({0.0f, 1.0f, 0.0f}, euler.y);
  const Quat qz = axisAngle({0.0f, 0.0f, 1.0f}, euler.z);
  return normalize(qz * qy * qx);
}

inline Vec3 rotate(const Quat rotation, const Vec3 value) {
  const Quat q = normalize(rotation);
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * dot(u, value)) + value * (s * s - dot(u, u)) +
         cross(u, value) * (2.0f * s);
}

inline Mat3 mat3FromQuat(const Quat value) {
  const Quat q = normalize(value);
  const float xx = q.x * q.x;
  const float yy = q.y * q.y;
  const float zz = q.z * q.z;
  const float xz = q.x * q.z;
  const float xy = q.x * q.y;
  const float yz = q.y * q.z;
  const float wx = q.w * q.x;
  const float wy = q.w * q.y;
  const float wz = q.w * q.z;

  Mat3 out = identity3();
  out.m[0] = 1.0f - 2.0f * (yy + zz);
  out.m[1] = 2.0f * (xy + wz);
  out.m[2] = 2.0f * (xz - wy);
  out.m[3] = 2.0f * (xy - wz);
  out.m[4] = 1.0f - 2.0f * (xx + zz);
  out.m[5] = 2.0f * (yz + wx);
  out.m[6] = 2.0f * (xz + wy);
  out.m[7] = 2.0f * (yz - wx);
  out.m[8] = 1.0f - 2.0f * (xx + yy);
  return out;
}

inline Mat4 mat4FromQuat(const Quat value) {
  return mat4FromMat3(mat3FromQuat(value));
}

inline Vec3 eulerXyz(const Quat value) {
  const Mat3 m = mat3FromQuat(value);
  const float sy = clamp(-at(m, 2, 0), -1.0f, 1.0f);
  const float y = std::asin(sy);
  const float cy = std::cos(y);
  if (std::abs(cy) > 0.00001f) {
    return {std::atan2(at(m, 2, 1), at(m, 2, 2)), y,
            std::atan2(at(m, 1, 0), at(m, 0, 0))};
  }
  return {0.0f, y, std::atan2(-at(m, 0, 1), at(m, 1, 1))};
}

inline Quat slerp(Quat a, Quat b, const float t) {
  a = normalize(a);
  b = normalize(b);
  float cos_theta = dot(a, b);
  if (cos_theta < 0.0f) {
    b = -b;
    cos_theta = -cos_theta;
  }
  if (cos_theta > 0.9995f) {
    return normalize(a * (1.0f - t) + b * t);
  }
  const float theta = std::acos(clamp(cos_theta, -1.0f, 1.0f));
  const float sin_theta = std::sin(theta);
  const float wa = std::sin((1.0f - t) * theta) / sin_theta;
  const float wb = std::sin(t * theta) / sin_theta;
  return normalize(a * wa + b * wb);
}

inline Quat quatLookAtRH(const Vec3 direction, const Vec3 up) {
  const Vec3 f = normalize(direction);
  const Vec3 s = normalize(cross(f, up));
  const Vec3 u = cross(s, f);
  const Mat3 basis = mat3FromColumns(s, u, -f);

  const float trace = at(basis, 0, 0) + at(basis, 1, 1) + at(basis, 2, 2);
  if (trace > 0.0f) {
    const float root = std::sqrt(trace + 1.0f);
    const float inv = 0.5f / root;
    return normalize(Quat{(at(basis, 2, 1) - at(basis, 1, 2)) * inv,
                          (at(basis, 0, 2) - at(basis, 2, 0)) * inv,
                          (at(basis, 1, 0) - at(basis, 0, 1)) * inv, root * 0.5f});
  }

  if (at(basis, 0, 0) > at(basis, 1, 1) && at(basis, 0, 0) > at(basis, 2, 2)) {
    const float root = std::sqrt(1.0f + at(basis, 0, 0) - at(basis, 1, 1) -
                                 at(basis, 2, 2));
    const float inv = 0.5f / root;
    return normalize(Quat{root * 0.5f, (at(basis, 0, 1) + at(basis, 1, 0)) * inv,
                          (at(basis, 0, 2) + at(basis, 2, 0)) * inv,
                          (at(basis, 2, 1) - at(basis, 1, 2)) * inv});
  }
  if (at(basis, 1, 1) > at(basis, 2, 2)) {
    const float root = std::sqrt(1.0f + at(basis, 1, 1) - at(basis, 0, 0) -
                                 at(basis, 2, 2));
    const float inv = 0.5f / root;
    return normalize(Quat{(at(basis, 0, 1) + at(basis, 1, 0)) * inv, root * 0.5f,
                          (at(basis, 1, 2) + at(basis, 2, 1)) * inv,
                          (at(basis, 0, 2) - at(basis, 2, 0)) * inv});
  }
  const float root =
      std::sqrt(1.0f + at(basis, 2, 2) - at(basis, 0, 0) - at(basis, 1, 1));
  const float inv = 0.5f / root;
  return normalize(Quat{(at(basis, 0, 2) + at(basis, 2, 0)) * inv,
                        (at(basis, 1, 2) + at(basis, 2, 1)) * inv, root * 0.5f,
                        (at(basis, 1, 0) - at(basis, 0, 1)) * inv});
}

} // namespace aster
