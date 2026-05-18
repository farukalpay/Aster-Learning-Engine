// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace aster {

template <typename T> struct Vec2T {
  using value_type = T;

  T x{};
  T y{};

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    return index == 0u ? x : y;
  }

  [[nodiscard]] constexpr const T &operator[](const std::size_t index) const {
    return index == 0u ? x : y;
  }
};

template <typename T> struct Vec3T {
  using value_type = T;

  T x{};
  T y{};
  T z{};

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }

  [[nodiscard]] constexpr const T &operator[](const std::size_t index) const {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }
};

template <typename T> struct Vec4T {
  using value_type = T;

  T x{};
  T y{};
  T z{};
  T w{};

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

using Vec2 = Vec2T<float>;
using Vec3 = Vec3T<float>;
using Vec4 = Vec4T<float>;
using DVec2 = Vec2T<double>;
using DVec3 = Vec3T<double>;
using DVec4 = Vec4T<double>;
using IVec2 = Vec2T<std::int32_t>;
using IVec3 = Vec3T<std::int32_t>;
using IVec4 = Vec4T<std::int32_t>;
using UVec2 = Vec2T<std::uint32_t>;
using UVec3 = Vec3T<std::uint32_t>;
using UVec4 = Vec4T<std::uint32_t>;
using BVec2 = Vec2T<bool>;
using BVec3 = Vec3T<bool>;
using BVec4 = Vec4T<bool>;

struct Degrees {
  float value = 0.0f;
};

struct Radians {
  float value = 0.0f;
};

template <typename T> inline constexpr Vec2T<T> operator+(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

template <typename T> inline constexpr Vec2T<T> operator-(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}

template <typename T> inline constexpr Vec2T<T> operator-(const Vec2T<T> value) {
  return {-value.x, -value.y};
}

template <typename T, typename S>
inline constexpr Vec2T<T> operator*(const Vec2T<T> value, const S scalar) {
  return {static_cast<T>(value.x * scalar), static_cast<T>(value.y * scalar)};
}

template <typename T, typename S>
inline constexpr Vec2T<T> operator*(const S scalar, const Vec2T<T> value) {
  return value * scalar;
}

template <typename T> inline constexpr Vec2T<T> operator*(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return {lhs.x * rhs.x, lhs.y * rhs.y};
}

template <typename T, typename S>
inline constexpr Vec2T<T> operator/(const Vec2T<T> value, const S scalar) {
  return {static_cast<T>(value.x / scalar), static_cast<T>(value.y / scalar)};
}

template <typename T> inline constexpr Vec3T<T> operator+(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

template <typename T> inline constexpr Vec3T<T> operator-(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

template <typename T> inline constexpr Vec3T<T> operator-(const Vec3T<T> value) {
  return {-value.x, -value.y, -value.z};
}

template <typename T, typename S>
inline constexpr Vec3T<T> operator*(const Vec3T<T> value, const S scalar) {
  return {static_cast<T>(value.x * scalar), static_cast<T>(value.y * scalar),
          static_cast<T>(value.z * scalar)};
}

template <typename T, typename S>
inline constexpr Vec3T<T> operator*(const S scalar, const Vec3T<T> value) {
  return value * scalar;
}

template <typename T> inline constexpr Vec3T<T> operator*(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
}

template <typename T, typename S>
inline constexpr Vec3T<T> operator/(const Vec3T<T> value, const S scalar) {
  return {static_cast<T>(value.x / scalar), static_cast<T>(value.y / scalar),
          static_cast<T>(value.z / scalar)};
}

template <typename T> inline constexpr Vec4T<T> operator+(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

template <typename T> inline constexpr Vec4T<T> operator-(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

template <typename T> inline constexpr Vec4T<T> operator-(const Vec4T<T> value) {
  return {-value.x, -value.y, -value.z, -value.w};
}

template <typename T, typename S>
inline constexpr Vec4T<T> operator*(const Vec4T<T> value, const S scalar) {
  return {static_cast<T>(value.x * scalar), static_cast<T>(value.y * scalar),
          static_cast<T>(value.z * scalar), static_cast<T>(value.w * scalar)};
}

template <typename T, typename S>
inline constexpr Vec4T<T> operator*(const S scalar, const Vec4T<T> value) {
  return value * scalar;
}

template <typename T> inline constexpr Vec4T<T> operator*(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};
}

template <typename T, typename S>
inline constexpr Vec4T<T> operator/(const Vec4T<T> value, const S scalar) {
  return {static_cast<T>(value.x / scalar), static_cast<T>(value.y / scalar),
          static_cast<T>(value.z / scalar), static_cast<T>(value.w / scalar)};
}

template <typename T> inline constexpr Vec2T<T> &operator+=(Vec2T<T> &lhs, const Vec2T<T> rhs) {
  lhs = lhs + rhs;
  return lhs;
}

template <typename T> inline constexpr Vec3T<T> &operator+=(Vec3T<T> &lhs, const Vec3T<T> rhs) {
  lhs = lhs + rhs;
  return lhs;
}

template <typename T> inline constexpr Vec4T<T> &operator+=(Vec4T<T> &lhs, const Vec4T<T> rhs) {
  lhs = lhs + rhs;
  return lhs;
}

template <typename T> inline constexpr Vec2T<T> &operator-=(Vec2T<T> &lhs, const Vec2T<T> rhs) {
  lhs = lhs - rhs;
  return lhs;
}

template <typename T> inline constexpr Vec3T<T> &operator-=(Vec3T<T> &lhs, const Vec3T<T> rhs) {
  lhs = lhs - rhs;
  return lhs;
}

template <typename T> inline constexpr Vec4T<T> &operator-=(Vec4T<T> &lhs, const Vec4T<T> rhs) {
  lhs = lhs - rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec2T<T> &operator*=(Vec2T<T> &lhs, const S rhs) {
  lhs = lhs * rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec3T<T> &operator*=(Vec3T<T> &lhs, const S rhs) {
  lhs = lhs * rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec4T<T> &operator*=(Vec4T<T> &lhs, const S rhs) {
  lhs = lhs * rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec2T<T> &operator/=(Vec2T<T> &lhs, const S rhs) {
  lhs = lhs / rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec3T<T> &operator/=(Vec3T<T> &lhs, const S rhs) {
  lhs = lhs / rhs;
  return lhs;
}

template <typename T, typename S> inline constexpr Vec4T<T> &operator/=(Vec4T<T> &lhs, const S rhs) {
  lhs = lhs / rhs;
  return lhs;
}

template <typename T> inline constexpr bool operator==(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

template <typename T> inline constexpr bool operator==(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

template <typename T> inline constexpr bool operator==(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

template <typename T> inline constexpr bool operator!=(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return !(lhs == rhs);
}

template <typename T> inline constexpr bool operator!=(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return !(lhs == rhs);
}

template <typename T> inline constexpr bool operator!=(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return !(lhs == rhs);
}

template <typename T> inline constexpr T dot(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

template <typename T> inline constexpr T dot(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

template <typename T> inline constexpr T dot(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <typename T> inline constexpr Vec3T<T> cross(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

template <typename T> inline T length(const Vec2T<T> value) {
  return std::sqrt(dot(value, value));
}

template <typename T> inline T length(const Vec3T<T> value) {
  return std::sqrt(dot(value, value));
}

template <typename T> inline T length(const Vec4T<T> value) {
  return std::sqrt(dot(value, value));
}

inline float length(const Vec2 value) {
  return length<float>(value);
}

inline float length(const Vec3 value) {
  return length<float>(value);
}

template <typename T> inline T distance(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return length(lhs - rhs);
}

template <typename T> inline T distance(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return length(lhs - rhs);
}

template <typename T> inline T distance(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return length(lhs - rhs);
}

template <typename T> inline Vec2T<T> normalize(const Vec2T<T> value) {
  const T len = length(value);
  if (len <= static_cast<T>(0.000001)) {
    return {};
  }
  return value / len;
}

template <typename T> inline Vec3T<T> normalize(const Vec3T<T> value) {
  const T len = length(value);
  if (len <= static_cast<T>(0.000001)) {
    return {};
  }
  return value / len;
}

template <typename T> inline Vec4T<T> normalize(const Vec4T<T> value) {
  const T len = length(value);
  if (len <= static_cast<T>(0.000001)) {
    return {};
  }
  return value / len;
}

inline Vec2 normalize(const Vec2 value) {
  return normalize<float>(value);
}

inline Vec3 normalize(const Vec3 value) {
  return normalize<float>(value);
}

template <typename T> inline constexpr T min(const T lhs, const T rhs) {
  return lhs < rhs ? lhs : rhs;
}

template <typename T> inline constexpr T max(const T lhs, const T rhs) {
  return lhs > rhs ? lhs : rhs;
}

template <typename T> inline constexpr Vec2T<T> min(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y)};
}

template <typename T> inline constexpr Vec3T<T> min(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z)};
}

template <typename T> inline constexpr Vec4T<T> min(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return {min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z), min(lhs.w, rhs.w)};
}

template <typename T> inline constexpr Vec2T<T> max(const Vec2T<T> lhs, const Vec2T<T> rhs) {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y)};
}

template <typename T> inline constexpr Vec3T<T> max(const Vec3T<T> lhs, const Vec3T<T> rhs) {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z)};
}

template <typename T> inline constexpr Vec4T<T> max(const Vec4T<T> lhs, const Vec4T<T> rhs) {
  return {max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z), max(lhs.w, rhs.w)};
}

template <typename T> inline constexpr T clamp(const T value, const T low, const T high) {
  return value < low ? low : (value > high ? high : value);
}

template <typename T> inline constexpr Vec2T<T> clamp(const Vec2T<T> value, const T low, const T high) {
  return {clamp(value.x, low, high), clamp(value.y, low, high)};
}

template <typename T> inline constexpr Vec3T<T> clamp(const Vec3T<T> value, const T low, const T high) {
  return {clamp(value.x, low, high), clamp(value.y, low, high), clamp(value.z, low, high)};
}

template <typename T> inline constexpr Vec4T<T> clamp(const Vec4T<T> value, const T low, const T high) {
  return {clamp(value.x, low, high), clamp(value.y, low, high), clamp(value.z, low, high),
          clamp(value.w, low, high)};
}

template <typename T> inline constexpr Vec2T<T> clamp(const Vec2T<T> value, const Vec2T<T> low,
                                                       const Vec2T<T> high) {
  return {clamp(value.x, low.x, high.x), clamp(value.y, low.y, high.y)};
}

template <typename T> inline constexpr Vec3T<T> clamp(const Vec3T<T> value, const Vec3T<T> low,
                                                       const Vec3T<T> high) {
  return {clamp(value.x, low.x, high.x), clamp(value.y, low.y, high.y),
          clamp(value.z, low.z, high.z)};
}

template <typename T> inline constexpr Vec4T<T> clamp(const Vec4T<T> value, const Vec4T<T> low,
                                                       const Vec4T<T> high) {
  return {clamp(value.x, low.x, high.x), clamp(value.y, low.y, high.y),
          clamp(value.z, low.z, high.z), clamp(value.w, low.w, high.w)};
}

template <typename T> inline constexpr T saturate(const T value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T> inline constexpr Vec2T<T> saturate(const Vec2T<T> value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T> inline constexpr Vec3T<T> saturate(const Vec3T<T> value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T> inline constexpr Vec4T<T> saturate(const Vec4T<T> value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T> inline constexpr T mix(const T a, const T b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T> inline constexpr Vec2T<T> mix(const Vec2T<T> a, const Vec2T<T> b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T> inline constexpr Vec3T<T> mix(const Vec3T<T> a, const Vec3T<T> b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T> inline constexpr Vec4T<T> mix(const Vec4T<T> a, const Vec4T<T> b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T> inline constexpr T step(const T edge, const T value) {
  return value < edge ? static_cast<T>(0) : static_cast<T>(1);
}

template <typename T> inline constexpr Vec2T<T> step(const Vec2T<T> edge, const Vec2T<T> value) {
  return {step(edge.x, value.x), step(edge.y, value.y)};
}

template <typename T> inline constexpr Vec3T<T> step(const Vec3T<T> edge, const Vec3T<T> value) {
  return {step(edge.x, value.x), step(edge.y, value.y), step(edge.z, value.z)};
}

template <typename T> inline constexpr Vec4T<T> step(const Vec4T<T> edge, const Vec4T<T> value) {
  return {step(edge.x, value.x), step(edge.y, value.y), step(edge.z, value.z),
          step(edge.w, value.w)};
}

template <typename T> inline T smoothstep(const T edge0, const T edge1, const T value) {
  const T range = max(edge1 - edge0, static_cast<T>(0.000001));
  const T t = saturate((value - edge0) / range);
  return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
}

template <typename T> inline Vec2T<T> smoothstep(const Vec2T<T> edge0, const Vec2T<T> edge1,
                                                  const Vec2T<T> value) {
  return {smoothstep(edge0.x, edge1.x, value.x), smoothstep(edge0.y, edge1.y, value.y)};
}

template <typename T> inline Vec3T<T> smoothstep(const Vec3T<T> edge0, const Vec3T<T> edge1,
                                                  const Vec3T<T> value) {
  return {smoothstep(edge0.x, edge1.x, value.x), smoothstep(edge0.y, edge1.y, value.y),
          smoothstep(edge0.z, edge1.z, value.z)};
}

template <typename T> inline Vec4T<T> smoothstep(const Vec4T<T> edge0, const Vec4T<T> edge1,
                                                  const Vec4T<T> value) {
  return {smoothstep(edge0.x, edge1.x, value.x), smoothstep(edge0.y, edge1.y, value.y),
          smoothstep(edge0.z, edge1.z, value.z), smoothstep(edge0.w, edge1.w, value.w)};
}

template <typename T> inline T fract(const T value) {
  return static_cast<T>(value - std::floor(value));
}

template <typename T> inline Vec2T<T> fract(const Vec2T<T> value) {
  return {fract(value.x), fract(value.y)};
}

template <typename T> inline Vec3T<T> fract(const Vec3T<T> value) {
  return {fract(value.x), fract(value.y), fract(value.z)};
}

template <typename T> inline Vec4T<T> fract(const Vec4T<T> value) {
  return {fract(value.x), fract(value.y), fract(value.z), fract(value.w)};
}

template <typename T> inline Vec2T<T> floor(const Vec2T<T> value) {
  return {static_cast<T>(std::floor(value.x)), static_cast<T>(std::floor(value.y))};
}

template <typename T> inline Vec3T<T> floor(const Vec3T<T> value) {
  return {static_cast<T>(std::floor(value.x)), static_cast<T>(std::floor(value.y)),
          static_cast<T>(std::floor(value.z))};
}

template <typename T> inline Vec4T<T> floor(const Vec4T<T> value) {
  return {static_cast<T>(std::floor(value.x)), static_cast<T>(std::floor(value.y)),
          static_cast<T>(std::floor(value.z)), static_cast<T>(std::floor(value.w))};
}

template <typename T> inline Vec2T<T> ceil(const Vec2T<T> value) {
  return {static_cast<T>(std::ceil(value.x)), static_cast<T>(std::ceil(value.y))};
}

template <typename T> inline Vec3T<T> ceil(const Vec3T<T> value) {
  return {static_cast<T>(std::ceil(value.x)), static_cast<T>(std::ceil(value.y)),
          static_cast<T>(std::ceil(value.z))};
}

template <typename T> inline Vec4T<T> ceil(const Vec4T<T> value) {
  return {static_cast<T>(std::ceil(value.x)), static_cast<T>(std::ceil(value.y)),
          static_cast<T>(std::ceil(value.z)), static_cast<T>(std::ceil(value.w))};
}

template <typename T> inline constexpr Vec3T<T> reflect(const Vec3T<T> incident, const Vec3T<T> normal) {
  return incident - normal * (static_cast<T>(2) * dot(normal, incident));
}

template <typename T>
inline Vec3T<T> refract(const Vec3T<T> incident, const Vec3T<T> normal, const T eta) {
  const T d = dot(normal, incident);
  const T k = static_cast<T>(1) - eta * eta * (static_cast<T>(1) - d * d);
  if (k < static_cast<T>(0)) {
    return {};
  }
  return incident * eta - normal * (eta * d + std::sqrt(k));
}

template <typename T>
inline constexpr Vec3T<T> faceforward(const Vec3T<T> normal, const Vec3T<T> incident,
                                      const Vec3T<T> reference) {
  return dot(reference, incident) < static_cast<T>(0) ? normal : -normal;
}

inline constexpr float pi() {
  return 3.14159265358979323846f;
}

inline constexpr float tau() {
  return 6.28318530717958647692f;
}

inline constexpr float radians(const float degrees) {
  return degrees * (pi() / 180.0f);
}

inline constexpr float radians(const Degrees degrees) {
  return radians(degrees.value);
}

inline constexpr float degrees(const float radians_value) {
  return radians_value * (180.0f / pi());
}

inline constexpr float degrees(const Radians radians_value) {
  return degrees(radians_value.value);
}

struct Ray3 {
  Vec3 origin{};
  Vec3 direction{0.0f, 0.0f, -1.0f};
};

struct Plane3 {
  Vec3 normal{0.0f, 1.0f, 0.0f};
  float distance = 0.0f;
};

struct Aabb3 {
  Vec3 min{};
  Vec3 max{};
};

struct Sphere3 {
  Vec3 center{};
  float radius = 1.0f;
};

inline Aabb3 aabbFromCenterHalfExtents(const Vec3 center, const Vec3 half_extents) {
  return {center - half_extents, center + half_extents};
}

inline Vec3 center(const Aabb3 box) {
  return (box.min + box.max) * 0.5f;
}

inline Vec3 halfExtents(const Aabb3 box) {
  return (box.max - box.min) * 0.5f;
}

inline bool contains(const Aabb3 box, const Vec3 point) {
  return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
         point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

inline Plane3 planeFromPointNormal(const Vec3 point, const Vec3 normal) {
  const Vec3 safe_normal = normalize(normal);
  return {safe_normal, -dot(safe_normal, point)};
}

inline float signedDistance(const Plane3 plane, const Vec3 point) {
  return dot(plane.normal, point) + plane.distance;
}

} // namespace aster
