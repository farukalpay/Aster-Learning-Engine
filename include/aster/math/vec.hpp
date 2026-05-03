// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cmath>

namespace aster {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Vec4 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;
};

inline Vec2 operator+(const Vec2 lhs, const Vec2 rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vec2 operator-(const Vec2 lhs, const Vec2 rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vec2 operator*(const Vec2 value, const float scalar) {
  return {value.x * scalar, value.y * scalar};
}

inline Vec2 operator/(const Vec2 value, const float scalar) {
  return {value.x / scalar, value.y / scalar};
}

inline Vec3 operator+(const Vec3 lhs, const Vec3 rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vec3 operator-(const Vec3 lhs, const Vec3 rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vec3 operator*(const Vec3 value, const float scalar) {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

inline Vec3 operator*(const float scalar, const Vec3 value) {
  return value * scalar;
}

inline Vec3 operator*(const Vec3 lhs, const Vec3 rhs) {
  return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
}

inline Vec3 operator/(const Vec3 value, const float scalar) {
  return {value.x / scalar, value.y / scalar, value.z / scalar};
}

inline float dot(const Vec3 lhs, const Vec3 rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline float dot(const Vec2 lhs, const Vec2 rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline Vec3 cross(const Vec3 lhs, const Vec3 rhs) {
  return {
      lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.z * rhs.x - lhs.x * rhs.z,
      lhs.x * rhs.y - lhs.y * rhs.x,
  };
}

inline float length(const Vec3 value) {
  return std::sqrt(dot(value, value));
}

inline float length(const Vec2 value) {
  return std::sqrt(dot(value, value));
}

inline Vec3 normalize(const Vec3 value) {
  const float len = length(value);
  if (len <= 0.000001f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return value / len;
}

inline Vec2 normalize(const Vec2 value) {
  const float len = length(value);
  if (len <= 0.000001f) {
    return {0.0f, 0.0f};
  }
  return value / len;
}

inline float clamp(const float value, const float low, const float high) {
  return value < low ? low : (value > high ? high : value);
}

inline Vec3 clamp(const Vec3 value, const float low, const float high) {
  return {clamp(value.x, low, high), clamp(value.y, low, high), clamp(value.z, low, high)};
}

inline float radians(const float degrees) {
  constexpr float kPi = 3.14159265358979323846f;
  return degrees * (kPi / 180.0f);
}

} // namespace aster
