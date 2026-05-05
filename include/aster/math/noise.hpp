// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cmath>
#include <cstdint>

namespace aster {

inline float fract(const float value) {
  return value - std::floor(value);
}

inline float hash01(const std::uint32_t value) {
  std::uint32_t x = value;
  x ^= x >> 16u;
  x *= 0x7feb352du;
  x ^= x >> 15u;
  x *= 0x846ca68bu;
  x ^= x >> 16u;
  return static_cast<float>(x & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

inline std::uint32_t hashGrid(const int x, const int y, const int z, const std::uint32_t seed) {
  std::uint32_t h = seed ^ 0x9e3779b9u;
  h ^= static_cast<std::uint32_t>(x) + 0x85ebca6bu + (h << 6u) + (h >> 2u);
  h ^= static_cast<std::uint32_t>(y) + 0xc2b2ae35u + (h << 6u) + (h >> 2u);
  h ^= static_cast<std::uint32_t>(z) + 0x27d4eb2fu + (h << 6u) + (h >> 2u);
  return h;
}

inline float valueNoise(const Vec3 point, const std::uint32_t seed = 0u) {
  const int ix = static_cast<int>(std::floor(point.x));
  const int iy = static_cast<int>(std::floor(point.y));
  const int iz = static_cast<int>(std::floor(point.z));
  Vec3 f{fract(point.x), fract(point.y), fract(point.z)};
  f = f * f * (Vec3{3.0f, 3.0f, 3.0f} - f * 2.0f);

  const auto sample = [&](const int x, const int y, const int z) {
    return hash01(hashGrid(ix + x, iy + y, iz + z, seed));
  };
  const float n000 = sample(0, 0, 0);
  const float n100 = sample(1, 0, 0);
  const float n010 = sample(0, 1, 0);
  const float n110 = sample(1, 1, 0);
  const float n001 = sample(0, 0, 1);
  const float n101 = sample(1, 0, 1);
  const float n011 = sample(0, 1, 1);
  const float n111 = sample(1, 1, 1);

  const float nx00 = n000 + (n100 - n000) * f.x;
  const float nx10 = n010 + (n110 - n010) * f.x;
  const float nx01 = n001 + (n101 - n001) * f.x;
  const float nx11 = n011 + (n111 - n011) * f.x;
  const float nxy0 = nx00 + (nx10 - nx00) * f.y;
  const float nxy1 = nx01 + (nx11 - nx01) * f.y;
  return nxy0 + (nxy1 - nxy0) * f.z;
}

inline float fractalNoise(Vec3 point, const std::uint32_t seed = 0u, const int octaves = 4) {
  float sum = 0.0f;
  float amplitude = 0.5f;
  float scale = 1.0f;
  float amplitude_sum = 0.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += valueNoise(point * scale, seed + static_cast<std::uint32_t>(octave) * 7919u) * amplitude;
    amplitude_sum += amplitude;
    scale *= 2.03f;
    amplitude *= 0.5f;
  }
  return amplitude_sum > 0.0f ? sum / amplitude_sum : 0.0f;
}

inline float ridgedNoise(const Vec3 point, const std::uint32_t seed = 0u, const int octaves = 4) {
  return 1.0f - std::abs(fractalNoise(point, seed, octaves) * 2.0f - 1.0f);
}

} // namespace aster
