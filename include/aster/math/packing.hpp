// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/quat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aster {

[[nodiscard]] inline std::uint16_t packUnorm16(const float value) {
  return static_cast<std::uint16_t>(
      std::lround(clamp(value, 0.0f, 1.0f) * static_cast<float>(0xffffu)));
}

[[nodiscard]] inline float unpackUnorm16(const std::uint16_t value) {
  return static_cast<float>(value) * (1.0f / static_cast<float>(0xffffu));
}

[[nodiscard]] inline std::uint16_t packSnorm16(const float value) {
  const int packed = static_cast<int>(std::lround(clamp(value, -1.0f, 1.0f) * 32767.0f));
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(packed, -32767, 32767)));
}

[[nodiscard]] inline float unpackSnorm16(const std::uint16_t value) {
  const auto signed_value = static_cast<std::int16_t>(value);
  return std::max(-1.0f, static_cast<float>(signed_value) * (1.0f / 32767.0f));
}

[[nodiscard]] inline std::uint32_t packOctNormal(const Vec3 normal) {
  Vec3 n = normalizeOr(normal, {0.0f, 0.0f, 1.0f});
  const float inv_l1 = 1.0f / std::max(std::abs(n.x) + std::abs(n.y) + std::abs(n.z), 0.000001f);
  n = n * inv_l1;
  Vec2 encoded{n.x, n.y};
  if (n.z < 0.0f) {
    encoded = {(1.0f - std::abs(encoded.y)) * (encoded.x >= 0.0f ? 1.0f : -1.0f),
               (1.0f - std::abs(encoded.x)) * (encoded.y >= 0.0f ? 1.0f : -1.0f)};
  }
  return static_cast<std::uint32_t>(packSnorm16(encoded.x)) |
         (static_cast<std::uint32_t>(packSnorm16(encoded.y)) << 16u);
}

[[nodiscard]] inline Vec3 unpackOctNormal(const std::uint32_t packed) {
  Vec2 f{unpackSnorm16(static_cast<std::uint16_t>(packed & 0xffffu)),
         unpackSnorm16(static_cast<std::uint16_t>(packed >> 16u))};
  Vec3 n{f.x, f.y, 1.0f - std::abs(f.x) - std::abs(f.y)};
  if (n.z < 0.0f) {
    const Vec2 folded{(1.0f - std::abs(f.y)) * (f.x >= 0.0f ? 1.0f : -1.0f),
                      (1.0f - std::abs(f.x)) * (f.y >= 0.0f ? 1.0f : -1.0f)};
    n.x = folded.x;
    n.y = folded.y;
  }
  return normalizeOr(n, {0.0f, 0.0f, 1.0f});
}

struct PackedQuat48 {
  std::uint16_t a = 0u;
  std::uint16_t b = 0u;
  std::uint16_t c = 0u;
  std::uint8_t largest = 3u;
  bool negative_largest = false;
};

[[nodiscard]] inline PackedQuat48 packUnitQuat48(const Quat value) {
  Quat q = normalize(value);
  const float components[4] = {q.x, q.y, q.z, q.w};
  std::uint8_t largest = 0u;
  for (std::uint8_t i = 1u; i < 4u; ++i) {
    if (std::abs(components[i]) > std::abs(components[largest])) {
      largest = i;
    }
  }
  const bool negative_largest = components[largest] < 0.0f;
  float stored[3]{};
  int out = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == largest) {
      continue;
    }
    stored[out++] = negative_largest ? -components[i] : components[i];
  }
  return {packSnorm16(stored[0]), packSnorm16(stored[1]), packSnorm16(stored[2]), largest,
          negative_largest};
}

[[nodiscard]] inline Quat unpackUnitQuat48(const PackedQuat48 packed) {
  float components[4]{};
  const float stored[3] = {unpackSnorm16(packed.a), unpackSnorm16(packed.b),
                           unpackSnorm16(packed.c)};
  float omitted_sq = 1.0f;
  int in = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == packed.largest) {
      continue;
    }
    components[i] = stored[in++];
    omitted_sq -= components[i] * components[i];
  }
  components[packed.largest] = std::sqrt(std::max(omitted_sq, 0.0f));
  if (packed.negative_largest) {
    for (float &component : components) {
      component = -component;
    }
  }
  return normalize(Quat{components[0], components[1], components[2], components[3]});
}

} // namespace aster
