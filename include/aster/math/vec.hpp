// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/result.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace aster {

template <typename T, std::size_t N> struct Vec;

template <typename T> struct Vec<T, 2> {
  using value_type = T;
  static constexpr std::size_t dimensions = 2u;

  T x{};
  T y{};

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    assert(index < dimensions);
    return uncheckedAt(index);
  }

  [[nodiscard]] constexpr const T &operator[](const std::size_t index) const {
    assert(index < dimensions);
    return uncheckedAt(index);
  }

  [[nodiscard]] constexpr T &uncheckedAt(const std::size_t index) {
    return index == 0u ? x : y;
  }

  [[nodiscard]] constexpr const T &uncheckedAt(const std::size_t index) const {
    return index == 0u ? x : y;
  }
};

template <typename T> struct Vec<T, 3> {
  using value_type = T;
  static constexpr std::size_t dimensions = 3u;

  T x{};
  T y{};
  T z{};

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    assert(index < dimensions);
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }

  [[nodiscard]] constexpr const T &operator[](const std::size_t index) const {
    assert(index < dimensions);
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }

  [[nodiscard]] constexpr T &uncheckedAt(const std::size_t index) {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }

  [[nodiscard]] constexpr const T &uncheckedAt(const std::size_t index) const {
    if (index == 0u) {
      return x;
    }
    if (index == 1u) {
      return y;
    }
    return z;
  }
};

template <typename T> struct Vec<T, 4> {
  using value_type = T;
  static constexpr std::size_t dimensions = 4u;

  T x{};
  T y{};
  T z{};
  T w{};

  [[nodiscard]] constexpr T &operator[](const std::size_t index) {
    assert(index < dimensions);
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
    assert(index < dimensions);
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

  [[nodiscard]] constexpr T &uncheckedAt(const std::size_t index) {
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

  [[nodiscard]] constexpr const T &uncheckedAt(const std::size_t index) const {
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

template <typename T> using Vec2T = Vec<T, 2>;
template <typename T> using Vec3T = Vec<T, 3>;
template <typename T> using Vec4T = Vec<T, 4>;

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

template <typename Tag> struct SemanticVec3 {
  union {
    Vec3 value;
    struct {
      float x;
      float y;
      float z;
    };
  };

  constexpr SemanticVec3() : value{} {}
  constexpr SemanticVec3(const float x_value, const float y_value, const float z_value)
      : value{x_value, y_value, z_value} {}
  constexpr SemanticVec3(const Vec3 vec) : value(vec) {}
  constexpr SemanticVec3(const SemanticVec3 &) = default;
  constexpr SemanticVec3 &operator=(const SemanticVec3 &) = default;

  [[nodiscard]] constexpr operator Vec3() const {
    return value;
  }

  constexpr SemanticVec3 &operator=(const Vec3 vec) {
    value = vec;
    return *this;
  }

  constexpr SemanticVec3 &operator=(std::initializer_list<float> values) {
    std::size_t index = 0u;
    for (const float component : values) {
      if (index < 3u) {
        value[index] = component;
      }
      ++index;
    }
    assert(index == 3u);
    return *this;
  }
};

template <typename Tag> struct SemanticVec4 {
  union {
    Vec4 value;
    struct {
      float x;
      float y;
      float z;
      float w;
    };
  };

  constexpr SemanticVec4() : value{} {}
  constexpr SemanticVec4(const float x_value, const float y_value, const float z_value,
                         const float w_value)
      : value{x_value, y_value, z_value, w_value} {}
  constexpr SemanticVec4(const Vec4 vec) : value(vec) {}
  constexpr SemanticVec4(const SemanticVec4 &) = default;
  constexpr SemanticVec4 &operator=(const SemanticVec4 &) = default;

  [[nodiscard]] constexpr operator Vec4() const {
    return value;
  }

  constexpr SemanticVec4 &operator=(const Vec4 vec) {
    value = vec;
    return *this;
  }

  constexpr SemanticVec4 &operator=(std::initializer_list<float> values) {
    std::size_t index = 0u;
    for (const float component : values) {
      if (index < 4u) {
        value[index] = component;
      }
      ++index;
    }
    assert(index == 4u);
    return *this;
  }
};

template <typename Tag> struct SemanticScalar {
  float value = 0.0f;

  constexpr SemanticScalar() = default;
  constexpr explicit SemanticScalar(const float scalar) : value(scalar) {}

  [[nodiscard]] constexpr operator float() const {
    return value;
  }
};

struct Degrees {
  float value = 0.0f;
};

struct Radians {
  float value = 0.0f;
};

struct Meters {
  float value = 0.0f;
};

struct Seconds {
  float value = 0.0f;
};

struct WorldPointTag {};
struct LocalPointTag {};
struct ViewPointTag {};
struct ClipPointTag {};
struct NdcPointTag {};
struct ScreenPointTag {};
struct DirectionTag {};
struct NormalTag {};
struct LinearRgbTag {};
struct SrgbTag {};
struct HdrColorTag {};
struct EmissionColorTag {};
struct AlphaTag {};
struct LuminanceTag {};
struct LocalToWorldTag {};
struct WorldToViewTag {};
struct ViewToClipTag {};
struct WorldToClipTag {};
struct ClipToWorldTag {};

using WorldPoint = SemanticVec3<WorldPointTag>;
using LocalPoint = SemanticVec3<LocalPointTag>;
using ViewPoint = SemanticVec3<ViewPointTag>;
using ClipPoint = SemanticVec4<ClipPointTag>;
using NdcPoint = SemanticVec3<NdcPointTag>;
using ScreenPoint = SemanticVec3<ScreenPointTag>;
using Direction = SemanticVec3<DirectionTag>;
using Normal = SemanticVec3<NormalTag>;
using LinearRgb = SemanticVec3<LinearRgbTag>;
using Srgb = SemanticVec3<SrgbTag>;
using HdrColor = SemanticVec3<HdrColorTag>;
using EmissionColor = SemanticVec3<EmissionColorTag>;
using Alpha = SemanticScalar<AlphaTag>;
using Luminance = SemanticScalar<LuminanceTag>;

struct WorldRay {
  WorldPoint origin{};
  Direction direction{0.0f, 0.0f, -1.0f};
  float max_distance = 0.0f;
};

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> operator+(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = lhs[i] + rhs[i];
  }
  return out;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> operator-(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = lhs[i] - rhs[i];
  }
  return out;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> operator-(const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = -value[i];
  }
  return out;
}

template <typename T, std::size_t N, typename S>
[[nodiscard]] inline constexpr Vec<T, N> operator*(const Vec<T, N> value, const S scalar) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = static_cast<T>(value[i] * scalar);
  }
  return out;
}

template <typename T, std::size_t N, typename S>
[[nodiscard]] inline constexpr Vec<T, N> operator*(const S scalar, const Vec<T, N> value) {
  return value * scalar;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> operator*(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = lhs[i] * rhs[i];
  }
  return out;
}

template <typename T, std::size_t N, typename S>
[[nodiscard]] inline constexpr Vec<T, N> operator/(const Vec<T, N> value, const S scalar) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = static_cast<T>(value[i] / scalar);
  }
  return out;
}

template <typename T, std::size_t N>
inline constexpr Vec<T, N> &operator+=(Vec<T, N> &lhs, const Vec<T, N> rhs) {
  lhs = lhs + rhs;
  return lhs;
}

template <typename T, std::size_t N>
inline constexpr Vec<T, N> &operator-=(Vec<T, N> &lhs, const Vec<T, N> rhs) {
  lhs = lhs - rhs;
  return lhs;
}

template <typename T, std::size_t N, typename S>
inline constexpr Vec<T, N> &operator*=(Vec<T, N> &lhs, const S rhs) {
  lhs = lhs * rhs;
  return lhs;
}

template <typename T, std::size_t N, typename S>
inline constexpr Vec<T, N> &operator/=(Vec<T, N> &lhs, const S rhs) {
  lhs = lhs / rhs;
  return lhs;
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator+(const SemanticVec3<Tag> lhs,
                                                           const Vec3 rhs) {
  return SemanticVec3<Tag>{lhs.value + rhs};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator+(const Vec3 lhs,
                                                           const SemanticVec3<Tag> rhs) {
  return SemanticVec3<Tag>{lhs + rhs.value};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator+(const SemanticVec3<Tag> lhs,
                                                           const SemanticVec3<Tag> rhs) {
  return SemanticVec3<Tag>{lhs.value + rhs.value};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator-(const SemanticVec3<Tag> lhs,
                                                           const Vec3 rhs) {
  return SemanticVec3<Tag>{lhs.value - rhs};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator-(const SemanticVec3<Tag> lhs,
                                                           const SemanticVec3<Tag> rhs) {
  return SemanticVec3<Tag>{lhs.value - rhs.value};
}

template <typename Tag, typename S>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator*(const SemanticVec3<Tag> value,
                                                           const S scalar) {
  return SemanticVec3<Tag>{value.value * scalar};
}

template <typename Tag, typename S>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator*(const S scalar,
                                                           const SemanticVec3<Tag> value) {
  return SemanticVec3<Tag>{value.value * scalar};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator*(const SemanticVec3<Tag> lhs,
                                                           const Vec3 rhs) {
  return SemanticVec3<Tag>{lhs.value * rhs};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator*(const Vec3 lhs,
                                                           const SemanticVec3<Tag> rhs) {
  return SemanticVec3<Tag>{lhs * rhs.value};
}

template <typename Tag>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator*(const SemanticVec3<Tag> lhs,
                                                           const SemanticVec3<Tag> rhs) {
  return SemanticVec3<Tag>{lhs.value * rhs.value};
}

template <typename Tag, typename S>
[[nodiscard]] inline constexpr SemanticVec3<Tag> operator/(const SemanticVec3<Tag> value,
                                                           const S scalar) {
  return SemanticVec3<Tag>{value.value / scalar};
}

template <typename Tag>
inline constexpr SemanticVec3<Tag> &operator+=(SemanticVec3<Tag> &lhs, const Vec3 rhs) {
  lhs = lhs.value + rhs;
  return lhs;
}

template <typename Tag>
inline constexpr SemanticVec3<Tag> &operator+=(SemanticVec3<Tag> &lhs,
                                               const SemanticVec3<Tag> rhs) {
  lhs = lhs.value + rhs.value;
  return lhs;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr bool operator==(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  for (std::size_t i = 0; i < N; ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }
  return true;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr bool operator!=(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  return !(lhs == rhs);
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr MathResult<T> checkedAt(const Vec<T, N> value,
                                                       const std::size_t index) {
  if (index >= N) {
    return MathResult<T>::failure(MathError::InvalidArgument, "Vector index is out of range.");
  }
  return MathResult<T>::success(value.uncheckedAt(index));
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr T dot(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  T out{};
  for (std::size_t i = 0; i < N; ++i) {
    out += lhs[i] * rhs[i];
  }
  return out;
}

template <typename T> [[nodiscard]] inline constexpr Vec3T<T> cross(const Vec3T<T> lhs,
                                                                    const Vec3T<T> rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

template <typename T, std::size_t N> [[nodiscard]] inline constexpr T lengthSquared(const Vec<T, N> value) {
  return dot(value, value);
}

template <typename T, std::size_t N> [[nodiscard]] inline T length(const Vec<T, N> value) {
  return std::sqrt(lengthSquared(value));
}

inline float length(const Vec2 value) {
  return length<float, 2>(value);
}

inline float length(const Vec3 value) {
  return length<float, 3>(value);
}

template <typename T, std::size_t N>
[[nodiscard]] inline T distance(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  return length(lhs - rhs);
}

template <typename T, std::size_t N>
[[nodiscard]] inline MathResult<Vec<T, N>> safeNormalize(const Vec<T, N> value,
                                                         const T epsilon =
                                                             ScalarTraits<T>::defaultAbsoluteEpsilon()) {
  const T len = length(value);
  if (len <= epsilon) {
    return MathResult<Vec<T, N>>::failure(MathError::DegenerateInput,
                                          "Cannot normalize a near-zero vector.");
  }
  return MathResult<Vec<T, N>>::success(value / len);
}

template <typename T, std::size_t N>
[[nodiscard]] inline Vec<T, N> normalizeOr(const Vec<T, N> value, const Vec<T, N> fallback,
                                           const T epsilon =
                                               ScalarTraits<T>::defaultAbsoluteEpsilon()) {
  const MathResult<Vec<T, N>> result = safeNormalize(value, epsilon);
  return result ? result.value : fallback;
}

template <typename T, std::size_t N>
[[nodiscard]] inline Vec<T, N> normalize(const Vec<T, N> value) {
  return normalizeOr(value, {});
}

inline Vec2 normalize(const Vec2 value) {
  return normalize<float, 2>(value);
}

inline Vec3 normalize(const Vec3 value) {
  return normalize<float, 3>(value);
}

template <typename T> [[nodiscard]] inline constexpr T min(const T lhs, const T rhs) {
  return lhs < rhs ? lhs : rhs;
}

template <typename T> [[nodiscard]] inline constexpr T max(const T lhs, const T rhs) {
  return lhs > rhs ? lhs : rhs;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> min(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = min(lhs[i], rhs[i]);
  }
  return out;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> max(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = max(lhs[i], rhs[i]);
  }
  return out;
}

template <typename T> [[nodiscard]] inline constexpr T clamp(const T value, const T low, const T high) {
  return value < low ? low : (value > high ? high : value);
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> clamp(const Vec<T, N> value, const T low, const T high) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = clamp(value[i], low, high);
  }
  return out;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> clamp(const Vec<T, N> value, const Vec<T, N> low,
                                               const Vec<T, N> high) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = clamp(value[i], low[i], high[i]);
  }
  return out;
}

template <typename T> [[nodiscard]] inline constexpr T saturate(const T value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> saturate(const Vec<T, N> value) {
  return clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T> [[nodiscard]] inline constexpr T mix(const T a, const T b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> mix(const Vec<T, N> a, const Vec<T, N> b, const T t) {
  return a * (static_cast<T>(1) - t) + b * t;
}

template <typename T> [[nodiscard]] inline constexpr T step(const T edge, const T value) {
  return value < edge ? static_cast<T>(0) : static_cast<T>(1);
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr Vec<T, N> step(const Vec<T, N> edge, const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = step(edge[i], value[i]);
  }
  return out;
}

template <typename T> [[nodiscard]] inline T smoothstep(const T edge0, const T edge1, const T value) {
  const T range = max(edge1 - edge0, ScalarTraits<T>::defaultAbsoluteEpsilon());
  const T t = saturate((value - edge0) / range);
  return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
}

template <typename T, std::size_t N>
[[nodiscard]] inline Vec<T, N> smoothstep(const Vec<T, N> edge0, const Vec<T, N> edge1,
                                          const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = smoothstep(edge0[i], edge1[i], value[i]);
  }
  return out;
}

template <typename T> [[nodiscard]] inline T fract(const T value) {
  return static_cast<T>(value - std::floor(value));
}

template <typename T, std::size_t N> [[nodiscard]] inline Vec<T, N> fract(const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = fract(value[i]);
  }
  return out;
}

template <typename T, std::size_t N> [[nodiscard]] inline Vec<T, N> floor(const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = static_cast<T>(std::floor(value[i]));
  }
  return out;
}

template <typename T, std::size_t N> [[nodiscard]] inline Vec<T, N> ceil(const Vec<T, N> value) {
  Vec<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = static_cast<T>(std::ceil(value[i]));
  }
  return out;
}

template <typename T>
[[nodiscard]] inline constexpr Vec3T<T> reflect(const Vec3T<T> incident, const Vec3T<T> normal) {
  return incident - normal * (static_cast<T>(2) * dot(normal, incident));
}

template <typename T>
[[nodiscard]] inline Vec3T<T> refract(const Vec3T<T> incident, const Vec3T<T> normal,
                                      const T eta) {
  const T d = dot(normal, incident);
  const T k = static_cast<T>(1) - eta * eta * (static_cast<T>(1) - d * d);
  if (k < static_cast<T>(0)) {
    return {};
  }
  return incident * eta - normal * (eta * d + std::sqrt(k));
}

template <typename T>
[[nodiscard]] inline constexpr Vec3T<T> faceforward(const Vec3T<T> normal,
                                                    const Vec3T<T> incident,
                                                    const Vec3T<T> reference) {
  return dot(reference, incident) < static_cast<T>(0) ? normal : -normal;
}

template <typename T, std::size_t N> [[nodiscard]] inline bool allFinite(const Vec<T, N> value) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!isFiniteScalar(value[i])) {
      return false;
    }
  }
  return true;
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

template <typename T> [[nodiscard]] inline constexpr bool exactEqual(const T lhs, const T rhs) {
  return lhs == rhs;
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr bool exactEqual(const Vec<T, N> lhs, const Vec<T, N> rhs) {
  for (std::size_t i = 0; i < N; ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }
  return true;
}

template <typename Tag>
[[nodiscard]] inline constexpr bool exactEqual(const SemanticVec3<Tag> lhs,
                                               const SemanticVec3<Tag> rhs) {
  return exactEqual(lhs.value, rhs.value);
}

template <typename Tag>
[[nodiscard]] inline constexpr bool exactEqual(const SemanticVec4<Tag> lhs,
                                               const SemanticVec4<Tag> rhs) {
  return exactEqual(lhs.value, rhs.value);
}

template <typename T>
[[nodiscard]] inline constexpr bool nearEqual(const T lhs, const T rhs,
                                              const T absolute_epsilon =
                                                  ScalarTraits<T>::defaultAbsoluteEpsilon(),
                                              const T relative_epsilon =
                                                  ScalarTraits<T>::defaultRelativeEpsilon()) {
  return nearlyEqual(lhs, rhs, absolute_epsilon, relative_epsilon);
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr bool nearEqual(const Vec<T, N> lhs, const Vec<T, N> rhs,
                                              const T absolute_epsilon =
                                                  ScalarTraits<T>::defaultAbsoluteEpsilon(),
                                              const T relative_epsilon =
                                                  ScalarTraits<T>::defaultRelativeEpsilon()) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!nearEqual(lhs[i], rhs[i], absolute_epsilon, relative_epsilon)) {
      return false;
    }
  }
  return true;
}

template <typename Tag>
[[nodiscard]] inline constexpr bool nearEqual(const SemanticVec3<Tag> lhs,
                                              const SemanticVec3<Tag> rhs,
                                              const float absolute_epsilon =
                                                  ScalarTraits<float>::defaultAbsoluteEpsilon(),
                                              const float relative_epsilon =
                                                  ScalarTraits<float>::defaultRelativeEpsilon()) {
  return nearEqual(lhs.value, rhs.value, absolute_epsilon, relative_epsilon);
}

template <typename Tag>
[[nodiscard]] inline constexpr bool nearEqual(const SemanticVec4<Tag> lhs,
                                              const SemanticVec4<Tag> rhs,
                                              const float absolute_epsilon =
                                                  ScalarTraits<float>::defaultAbsoluteEpsilon(),
                                              const float relative_epsilon =
                                                  ScalarTraits<float>::defaultRelativeEpsilon()) {
  return nearEqual(lhs.value, rhs.value, absolute_epsilon, relative_epsilon);
}

template <typename T> [[nodiscard]] inline constexpr bool ulpsEqual(const T lhs, const T rhs,
                                                                    const std::uint64_t max_ulps) {
  return almostEqualUlps(lhs, rhs, max_ulps);
}

template <typename T, std::size_t N>
[[nodiscard]] inline constexpr bool ulpsEqual(const Vec<T, N> lhs, const Vec<T, N> rhs,
                                              const std::uint64_t max_ulps) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!ulpsEqual(lhs[i], rhs[i], max_ulps)) {
      return false;
    }
  }
  return true;
}

template <typename Tag>
[[nodiscard]] inline constexpr bool ulpsEqual(const SemanticVec3<Tag> lhs,
                                              const SemanticVec3<Tag> rhs,
                                              const std::uint64_t max_ulps) {
  return ulpsEqual(lhs.value, rhs.value, max_ulps);
}

template <typename Tag>
[[nodiscard]] inline constexpr bool ulpsEqual(const SemanticVec4<Tag> lhs,
                                              const SemanticVec4<Tag> rhs,
                                              const std::uint64_t max_ulps) {
  return ulpsEqual(lhs.value, rhs.value, max_ulps);
}

struct Ray3 {
  Vec3 origin{};
  Vec3 direction{0.0f, 0.0f, -1.0f};
  float max_distance = 0.0f;
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

[[nodiscard]] inline Aabb3 aabbFromCenterHalfExtents(const Vec3 center, const Vec3 half_extents) {
  return {center - half_extents, center + half_extents};
}

[[nodiscard]] inline Vec3 center(const Aabb3 box) {
  return (box.min + box.max) * 0.5f;
}

[[nodiscard]] inline Vec3 halfExtents(const Aabb3 box) {
  return (box.max - box.min) * 0.5f;
}

[[nodiscard]] inline bool contains(const Aabb3 box, const Vec3 point) {
  return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
         point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

[[nodiscard]] inline Plane3 planeFromPointNormal(const Vec3 point, const Vec3 normal) {
  const Vec3 safe_normal = normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  return {safe_normal, -dot(safe_normal, point)};
}

[[nodiscard]] inline float signedDistance(const Plane3 plane, const Vec3 point) {
  return dot(plane.normal, point) + plane.distance;
}

} // namespace aster
