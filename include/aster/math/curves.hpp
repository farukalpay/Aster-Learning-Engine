// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/easing.hpp"

#include <algorithm>
#include <vector>

namespace aster {

template <typename T>
[[nodiscard]] inline T bezierQuadratic(const T p0, const T p1, const T p2, const float t_value) {
  const float t = clamp(t_value, 0.0f, 1.0f);
  const float inv = 1.0f - t;
  return p0 * (inv * inv) + p1 * (2.0f * inv * t) + p2 * (t * t);
}

template <typename T>
[[nodiscard]] inline T bezierCubic(const T p0, const T p1, const T p2, const T p3,
                                   const float t_value) {
  const float t = clamp(t_value, 0.0f, 1.0f);
  const float inv = 1.0f - t;
  return p0 * (inv * inv * inv) + p1 * (3.0f * inv * inv * t) +
         p2 * (3.0f * inv * t * t) + p3 * (t * t * t);
}

template <typename T>
[[nodiscard]] inline T hermite(const T p0, const T tangent0, const T p1, const T tangent1,
                               const float t_value) {
  const float t = clamp(t_value, 0.0f, 1.0f);
  const float t2 = t * t;
  const float t3 = t2 * t;
  return p0 * (2.0f * t3 - 3.0f * t2 + 1.0f) + tangent0 * (t3 - 2.0f * t2 + t) +
         p1 * (-2.0f * t3 + 3.0f * t2) + tangent1 * (t3 - t2);
}

template <typename T>
[[nodiscard]] inline T catmullRom(const T p0, const T p1, const T p2, const T p3,
                                  const float t_value) {
  const float t = clamp(t_value, 0.0f, 1.0f);
  const float t2 = t * t;
  const float t3 = t2 * t;
  return (p1 * 2.0f + (p2 - p0) * t +
          (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
          (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) *
         0.5f;
}

struct ArcLengthSample {
  float t = 0.0f;
  float length = 0.0f;
};

template <typename SampleFn>
[[nodiscard]] inline std::vector<ArcLengthSample> buildArcLengthTable(SampleFn sample,
                                                                      const int segments) {
  const int safe_segments = std::max(segments, 1);
  std::vector<ArcLengthSample> table;
  table.reserve(static_cast<std::size_t>(safe_segments) + 1u);
  Vec3 previous = sample(0.0f);
  table.push_back({0.0f, 0.0f});
  float accumulated = 0.0f;
  for (int i = 1; i <= safe_segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(safe_segments);
    const Vec3 current = sample(t);
    accumulated += distance(previous, current);
    table.push_back({t, accumulated});
    previous = current;
  }
  return table;
}

[[nodiscard]] inline float arcLengthToT(const std::vector<ArcLengthSample> &table,
                                        const float target_length) {
  if (table.empty() || target_length <= 0.0f) {
    return 0.0f;
  }
  if (target_length >= table.back().length) {
    return table.back().t;
  }
  for (std::size_t i = 1; i < table.size(); ++i) {
    if (table[i].length >= target_length) {
      const ArcLengthSample a = table[i - 1u];
      const ArcLengthSample b = table[i];
      const float span = std::max(b.length - a.length, 0.000001f);
      return mix(a.t, b.t, clamp((target_length - a.length) / span, 0.0f, 1.0f));
    }
  }
  return table.back().t;
}

} // namespace aster
