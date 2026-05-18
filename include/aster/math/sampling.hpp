// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/hash.hpp"

#include <cmath>
#include <vector>

namespace aster {

class Pcg32 {
public:
  explicit Pcg32(const std::uint64_t seed = 0xA57E3u,
                 const std::uint64_t sequence = 0x853c49e6748fea9bull) {
    state_ = 0u;
    increment_ = (sequence << 1u) | 1u;
    (void)nextU32();
    state_ += seed;
    (void)nextU32();
  }

  [[nodiscard]] std::uint32_t nextU32() {
    const std::uint64_t old_state = state_;
    state_ = old_state * 6364136223846793005ull + increment_;
    const std::uint32_t xorshifted =
        static_cast<std::uint32_t>(((old_state >> 18u) ^ old_state) >> 27u);
    const std::uint32_t rot = static_cast<std::uint32_t>(old_state >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
  }

  [[nodiscard]] float uniform01() {
    return static_cast<float>(nextU32() >> 8u) * (1.0f / 16777216.0f);
  }

  [[nodiscard]] float uniform(const float low, const float high) {
    return low + (high - low) * uniform01();
  }

private:
  std::uint64_t state_ = 0u;
  std::uint64_t increment_ = 0u;
};

[[nodiscard]] inline Vec2 sampleDiskConcentric(const Vec2 u) {
  const Vec2 offset = u * 2.0f - Vec2{1.0f, 1.0f};
  if (offset.x == 0.0f && offset.y == 0.0f) {
    return {};
  }
  float radius = 0.0f;
  float theta = 0.0f;
  if (std::abs(offset.x) > std::abs(offset.y)) {
    radius = offset.x;
    theta = (pi() * 0.25f) * (offset.y / offset.x);
  } else {
    radius = offset.y;
    theta = (pi() * 0.5f) - (pi() * 0.25f) * (offset.x / offset.y);
  }
  return {radius * std::cos(theta), radius * std::sin(theta)};
}

[[nodiscard]] inline Vec3 sampleSphere(const Vec2 u) {
  const float z = 1.0f - 2.0f * clamp(u.x, 0.0f, 1.0f);
  const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
  const float phi = tau() * clamp(u.y, 0.0f, 1.0f);
  return {r * std::cos(phi), r * std::sin(phi), z};
}

[[nodiscard]] inline Vec3 sampleHemisphereCosine(const Vec2 u) {
  const Vec2 disk = sampleDiskConcentric(u);
  const float z = std::sqrt(std::max(0.0f, 1.0f - dot(disk, disk)));
  return {disk.x, disk.y, z};
}

[[nodiscard]] inline std::vector<Vec2> poissonDiskSamples(const Vec2 min_point,
                                                          const Vec2 max_point,
                                                          const float radius,
                                                          const int candidates,
                                                          const std::uint64_t seed) {
  std::vector<Vec2> samples;
  if (radius <= 0.0f || candidates <= 0 || max_point.x <= min_point.x || max_point.y <= min_point.y) {
    return samples;
  }
  Pcg32 rng(seed);
  const float radius_sq = radius * radius;
  for (int i = 0; i < candidates; ++i) {
    const Vec2 p{rng.uniform(min_point.x, max_point.x), rng.uniform(min_point.y, max_point.y)};
    bool accepted = true;
    for (const Vec2 existing : samples) {
      if (lengthSquared(existing - p) < radius_sq) {
        accepted = false;
        break;
      }
    }
    if (accepted) {
      samples.push_back(p);
    }
  }
  return samples;
}

} // namespace aster
