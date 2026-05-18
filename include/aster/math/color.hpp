// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

inline float srgbChannelToLinear(const float value) {
  const float channel = clamp(value, 0.0f, 1.0f);
  if (channel <= 0.04045f) {
    return channel / 12.92f;
  }
  return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

inline float linearChannelToSrgb(const float value) {
  const float channel = clamp(value, 0.0f, 1.0f);
  if (channel <= 0.0031308f) {
    return channel * 12.92f;
  }
  return 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
}

inline LinearRgb srgbToLinear(const Srgb color) {
  return {srgbChannelToLinear(color.x), srgbChannelToLinear(color.y),
          srgbChannelToLinear(color.z)};
}

inline Srgb linearToSrgb(const LinearRgb color) {
  return {linearChannelToSrgb(color.x), linearChannelToSrgb(color.y),
          linearChannelToSrgb(color.z)};
}

inline HdrColor hdrColor(const LinearRgb color, const float exposure = 1.0f) {
  return HdrColor{color.value * std::max(exposure, 0.0f)};
}

inline HdrColor emissionToHdr(const EmissionColor color, const float strength = 1.0f) {
  return HdrColor{color.value * std::max(strength, 0.0f)};
}

inline Luminance relativeLuminance(const LinearRgb color) {
  return Luminance{color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f};
}

inline Vec3 aces_tonemap(const Vec3 value) {
  constexpr float a = 2.51f;
  constexpr float b = 0.03f;
  constexpr float c = 2.43f;
  constexpr float d = 0.59f;
  constexpr float e = 0.14f;
  return clamp(Vec3{(value.x * (a * value.x + b)) / (value.x * (c * value.x + d) + e),
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
