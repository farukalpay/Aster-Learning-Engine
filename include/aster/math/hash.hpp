// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <bit>
#include <cstdint>

namespace aster {

[[nodiscard]] inline constexpr std::uint32_t hash32Mix(std::uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

[[nodiscard]] inline constexpr std::uint64_t hash64Mix(std::uint64_t value) {
  value ^= value >> 33u;
  value *= 0xff51afd7ed558ccdull;
  value ^= value >> 33u;
  value *= 0xc4ceb9fe1a85ec53ull;
  value ^= value >> 33u;
  return value;
}

[[nodiscard]] inline constexpr std::uint32_t hashCombine32(std::uint32_t seed,
                                                           const std::uint32_t value) {
  return hash32Mix(seed ^ (value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u)));
}

[[nodiscard]] inline constexpr std::uint64_t hashCombine64(std::uint64_t seed,
                                                           const std::uint64_t value) {
  return hash64Mix(seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u)));
}

[[nodiscard]] inline std::uint32_t stableHash32(const float value) {
  return hash32Mix(std::bit_cast<std::uint32_t>(value));
}

[[nodiscard]] inline std::uint64_t stableHash64(const double value) {
  return hash64Mix(std::bit_cast<std::uint64_t>(value));
}

[[nodiscard]] inline std::uint32_t stableHash32(const Vec2 value,
                                                std::uint32_t seed = 0xA57E0001u) {
  seed = hashCombine32(seed, stableHash32(value.x));
  return hashCombine32(seed, stableHash32(value.y));
}

[[nodiscard]] inline std::uint32_t stableHash32(const Vec3 value,
                                                std::uint32_t seed = 0xA57E0003u) {
  seed = hashCombine32(seed, stableHash32(value.x));
  seed = hashCombine32(seed, stableHash32(value.y));
  return hashCombine32(seed, stableHash32(value.z));
}

[[nodiscard]] inline std::uint64_t stableHash64(const Vec3 value,
                                                std::uint64_t seed = 0xA57E000000000003ull) {
  seed = hashCombine64(seed, static_cast<std::uint64_t>(stableHash32(value.x)));
  seed = hashCombine64(seed, static_cast<std::uint64_t>(stableHash32(value.y)));
  return hashCombine64(seed, static_cast<std::uint64_t>(stableHash32(value.z)));
}

} // namespace aster
