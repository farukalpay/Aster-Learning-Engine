// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/mat4.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aster {

template <typename T> struct QuatT {
  using value_type = T;

  T x = T{};
  T y = T{};
  T z = T{};
  T w = static_cast<T>(1);

  constexpr QuatT() = default;
  constexpr QuatT(const T x_value, const T y_value, const T z_value, const T w_value)
      : x(x_value), y(y_value), z(z_value), w(w_value) {}

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    if (index == 2u) {
      return z;
    }
    return w;
  }

  [[nodiscard]] constexpr const T &operator[](const std::size_t index) const {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    if (index == 2u) {
      return z;
    }
    return w;
  }
};

using Quat = QuatT<float>;
using DQuat = QuatT<double>;

template <typename T> struct DualQuatT {
  QuatT<T> real{};
  QuatT<T> dual{0, 0, 0, 0};
};

using DualQuat = DualQuatT<float>;
using DDualQuat = DualQuatT<double>;

template <typename T> [[nodiscard]] inline constexpr QuatT<T> identityQuatT() {
  return {};
}

[[nodiscard]] inline constexpr Quat identityQuat() {
  return {};
}

template <typename T> [[nodiscard]] inline constexpr T dot(const QuatT<T> lhs, const QuatT<T> rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <typename T> [[nodiscard]] inline T length(const QuatT<T> value) {
  return std::sqrt(dot(value, value));
}

template <typename T> [[nodiscard]] inline QuatT<T> normalize(const QuatT<T> value) {
  const T len = length(value);
  if (len <= ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    return identityQuatT<T>();
  }
  return {value.x / len, value.y / len, value.z / len, value.w / len};
}

template <typename T> [[nodiscard]] inline MathResult<QuatT<T>> safeNormalize(const QuatT<T> value) {
  const T len = length(value);
  if (len <= ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    return MathResult<QuatT<T>>::failure(MathError::DegenerateInput,
                                         "Cannot normalize a near-zero quaternion.",
                                         identityQuatT<T>());
  }
  return MathResult<QuatT<T>>::success({value.x / len, value.y / len, value.z / len, value.w / len});
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> conjugate(const QuatT<T> value) {
  return {-value.x, -value.y, -value.z, value.w};
}

template <typename T> [[nodiscard]] inline MathResult<QuatT<T>> inverse(const QuatT<T> value) {
  const T len_sq = dot(value, value);
  if (len_sq <= ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    return MathResult<QuatT<T>>::failure(MathError::DegenerateInput,
                                         "Quaternion inverse requires non-zero length.",
                                         identityQuatT<T>());
  }
  const QuatT<T> c = conjugate(value);
  return MathResult<QuatT<T>>::success({c.x / len_sq, c.y / len_sq, c.z / len_sq, c.w / len_sq});
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator-(const QuatT<T> value) {
  return {-value.x, -value.y, -value.z, -value.w};
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator*(const QuatT<T> lhs,
                                                                        const T scalar) {
  return {lhs.x * scalar, lhs.y * scalar, lhs.z * scalar, lhs.w * scalar};
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator*(const T scalar,
                                                                        const QuatT<T> lhs) {
  return lhs * scalar;
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator/(const QuatT<T> lhs,
                                                                        const T scalar) {
  return {lhs.x / scalar, lhs.y / scalar, lhs.z / scalar, lhs.w / scalar};
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator+(const QuatT<T> lhs,
                                                                        const QuatT<T> rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator-(const QuatT<T> lhs,
                                                                        const QuatT<T> rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

template <typename T> [[nodiscard]] inline constexpr QuatT<T> operator*(const QuatT<T> lhs,
                                                                        const QuatT<T> rhs) {
  return {
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

template <typename T>
[[nodiscard]] inline MathResult<QuatT<T>> axisAngleSafe(Vec<T, 3> axis, const T radians_value) {
  const MathResult<Vec<T, 3>> axis_result = safeNormalize(axis);
  if (!axis_result) {
    return MathResult<QuatT<T>>::failure(MathError::DegenerateInput,
                                         "Axis-angle quaternion requires a non-zero axis.",
                                         identityQuatT<T>());
  }
  axis = axis_result.value;
  const T half_angle = radians_value * static_cast<T>(0.5);
  const T s = std::sin(half_angle);
  return safeNormalize(QuatT<T>{axis.x * s, axis.y * s, axis.z * s, std::cos(half_angle)});
}

[[nodiscard]] inline Quat axisAngle(const Vec3 axis, const float radians_value) {
  return axisAngleSafe(axis, radians_value).value;
}

[[nodiscard]] inline Quat quatFromEulerXyz(const Vec3 euler) {
  const Quat qx = axisAngle({1.0f, 0.0f, 0.0f}, euler.x);
  const Quat qy = axisAngle({0.0f, 1.0f, 0.0f}, euler.y);
  const Quat qz = axisAngle({0.0f, 0.0f, 1.0f}, euler.z);
  return normalize(qz * qy * qx);
}

template <typename T> [[nodiscard]] inline Vec<T, 3> rotate(const QuatT<T> rotation,
                                                            const Vec<T, 3> value) {
  const QuatT<T> q = normalize(rotation);
  const Vec<T, 3> u{q.x, q.y, q.z};
  const T s = q.w;
  return u * (static_cast<T>(2) * dot(u, value)) + value * (s * s - dot(u, u)) +
         cross(u, value) * (static_cast<T>(2) * s);
}

[[nodiscard]] inline Mat3 mat3FromQuat(const Quat value) {
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

[[nodiscard]] inline Mat4 mat4FromQuat(const Quat value) {
  return mat4FromMat3(mat3FromQuat(value));
}

[[nodiscard]] inline Quat quatFromMat3(const Mat3 &basis) {
  const float trace = at(basis, 0, 0) + at(basis, 1, 1) + at(basis, 2, 2);
  if (trace > 0.0f) {
    const float root = std::sqrt(trace + 1.0f);
    const float inv = 0.5f / root;
    return normalize(Quat{(at(basis, 2, 1) - at(basis, 1, 2)) * inv,
                          (at(basis, 0, 2) - at(basis, 2, 0)) * inv,
                          (at(basis, 1, 0) - at(basis, 0, 1)) * inv, root * 0.5f});
  }
  if (at(basis, 0, 0) > at(basis, 1, 1) && at(basis, 0, 0) > at(basis, 2, 2)) {
    const float root =
        std::sqrt(1.0f + at(basis, 0, 0) - at(basis, 1, 1) - at(basis, 2, 2));
    const float inv = 0.5f / root;
    return normalize(Quat{root * 0.5f, (at(basis, 0, 1) + at(basis, 1, 0)) * inv,
                          (at(basis, 0, 2) + at(basis, 2, 0)) * inv,
                          (at(basis, 2, 1) - at(basis, 1, 2)) * inv});
  }
  if (at(basis, 1, 1) > at(basis, 2, 2)) {
    const float root =
        std::sqrt(1.0f + at(basis, 1, 1) - at(basis, 0, 0) - at(basis, 2, 2));
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

[[nodiscard]] inline Vec3 eulerXyz(const Quat value) {
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

template <typename T>
[[nodiscard]] inline QuatT<T> slerp(QuatT<T> a, QuatT<T> b, const T t) {
  a = normalize(a);
  b = normalize(b);
  T cos_theta = dot(a, b);
  if (cos_theta < T{}) {
    b = -b;
    cos_theta = -cos_theta;
  }
  if (cos_theta > static_cast<T>(0.9995)) {
    return normalize(a * (static_cast<T>(1) - t) + b * t);
  }
  const T theta = std::acos(clamp(cos_theta, static_cast<T>(-1), static_cast<T>(1)));
  const T sin_theta = std::sin(theta);
  const T wa = std::sin((static_cast<T>(1) - t) * theta) / sin_theta;
  const T wb = std::sin(t * theta) / sin_theta;
  return normalize(a * wa + b * wb);
}

[[nodiscard]] inline MathResult<Quat> quatLookAtRH(const Vec3 direction, const Vec3 up) {
  const MathResult<Vec3> f_result = safeNormalize(direction);
  if (!f_result) {
    return MathResult<Quat>::failure(MathError::DegenerateInput,
                                     "quatLookAt requires a non-zero direction.", identityQuat());
  }
  const Vec3 f = f_result.value;
  const MathResult<Vec3> s_result = safeNormalize(cross(f, up));
  if (!s_result) {
    return MathResult<Quat>::failure(MathError::DegenerateInput,
                                     "quatLookAt requires direction and up not to be parallel.",
                                     identityQuat());
  }
  const Vec3 s = s_result.value;
  const Vec3 u = cross(s, f);
  const Mat3 basis = mat3FromColumns(s, u, -f);

  const float trace = at(basis, 0, 0) + at(basis, 1, 1) + at(basis, 2, 2);
  if (trace > 0.0f) {
    const float root = std::sqrt(trace + 1.0f);
    const float inv = 0.5f / root;
    return MathResult<Quat>::success(normalize(Quat{(at(basis, 2, 1) - at(basis, 1, 2)) * inv,
                                                    (at(basis, 0, 2) - at(basis, 2, 0)) * inv,
                                                    (at(basis, 1, 0) - at(basis, 0, 1)) * inv,
                                                    root * 0.5f}));
  }

  if (at(basis, 0, 0) > at(basis, 1, 1) && at(basis, 0, 0) > at(basis, 2, 2)) {
    const float root =
        std::sqrt(1.0f + at(basis, 0, 0) - at(basis, 1, 1) - at(basis, 2, 2));
    const float inv = 0.5f / root;
    return MathResult<Quat>::success(normalize(Quat{root * 0.5f,
                                                    (at(basis, 0, 1) + at(basis, 1, 0)) * inv,
                                                    (at(basis, 0, 2) + at(basis, 2, 0)) * inv,
                                                    (at(basis, 2, 1) - at(basis, 1, 2)) * inv}));
  }
  if (at(basis, 1, 1) > at(basis, 2, 2)) {
    const float root =
        std::sqrt(1.0f + at(basis, 1, 1) - at(basis, 0, 0) - at(basis, 2, 2));
    const float inv = 0.5f / root;
    return MathResult<Quat>::success(normalize(Quat{(at(basis, 0, 1) + at(basis, 1, 0)) * inv,
                                                    root * 0.5f,
                                                    (at(basis, 1, 2) + at(basis, 2, 1)) * inv,
                                                    (at(basis, 0, 2) - at(basis, 2, 0)) * inv}));
  }
  const float root =
      std::sqrt(1.0f + at(basis, 2, 2) - at(basis, 0, 0) - at(basis, 1, 1));
  const float inv = 0.5f / root;
  return MathResult<Quat>::success(normalize(Quat{(at(basis, 0, 2) + at(basis, 2, 0)) * inv,
                                                  (at(basis, 1, 2) + at(basis, 2, 1)) * inv,
                                                  root * 0.5f,
                                                  (at(basis, 1, 0) - at(basis, 0, 1)) * inv}));
}

template <typename T> [[nodiscard]] inline QuatT<T> exp(const QuatT<T> q) {
  const Vec<T, 3> u{q.x, q.y, q.z};
  const T angle = length(u);
  if (angle < ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    return identityQuatT<T>();
  }
  const Vec<T, 3> v = u / angle;
  return {std::sin(angle) * v.x, std::sin(angle) * v.y, std::sin(angle) * v.z, std::cos(angle)};
}

template <typename T> [[nodiscard]] inline QuatT<T> log(const QuatT<T> q) {
  const Vec<T, 3> u{q.x, q.y, q.z};
  const T vec_len = length(u);
  if (vec_len < ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    if (q.w > T{}) {
      return {T{}, T{}, T{}, std::log(q.w)};
    }
    if (q.w < T{}) {
      return {pi(), T{}, T{}, std::log(-q.w)};
    }
    const T inf = std::numeric_limits<T>::infinity();
    return {inf, inf, inf, inf};
  }
  const T t = std::atan2(vec_len, q.w) / vec_len;
  const T len_sq = vec_len * vec_len + q.w * q.w;
  return {t * q.x, t * q.y, t * q.z, static_cast<T>(0.5) * std::log(len_sq)};
}

template <typename T> [[nodiscard]] inline QuatT<T> pow(const QuatT<T> q, const T exponent) {
  if (std::abs(exponent) <= ScalarTraits<T>::defaultAbsoluteEpsilon()) {
    return identityQuatT<T>();
  }
  return exp(log(q) * exponent);
}

[[nodiscard]] inline DualQuat dualQuatFromRotationTranslation(const Quat rotation,
                                                             const Vec3 translation_value) {
  const Quat real = normalize(rotation);
  const Quat t{translation_value.x, translation_value.y, translation_value.z, 0.0f};
  return {real, (t * real) * 0.5f};
}

[[nodiscard]] inline Vec3 transformPoint(const DualQuat dq, const Vec3 point) {
  const Quat real = normalize(dq.real);
  const MathResult<Quat> real_inv = inverse(real);
  const Quat translation_quat = dq.dual * real_inv.value * 2.0f;
  return rotate(real, point) + Vec3{translation_quat.x, translation_quat.y, translation_quat.z};
}

} // namespace aster
