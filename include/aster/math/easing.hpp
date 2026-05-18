// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

[[nodiscard]] inline float easeInQuad(const float value) {
  const float t = clamp(value, 0.0f, 1.0f);
  return t * t;
}

[[nodiscard]] inline float easeOutQuad(const float value) {
  const float t = clamp(value, 0.0f, 1.0f);
  return t * (2.0f - t);
}

[[nodiscard]] inline float easeInOutCubic(const float value) {
  const float t = clamp(value, 0.0f, 1.0f);
  return t < 0.5f ? 4.0f * t * t * t
                  : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

[[nodiscard]] inline float easeOutCubicMath(const float value) {
  const float t = clamp(value, 0.0f, 1.0f);
  const float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

[[nodiscard]] inline float dampingFactor(const float response, const float dt) {
  if (response <= 0.0f) {
    return 1.0f;
  }
  return 1.0f - std::exp(-std::max(response, 0.0f) * std::max(dt, 0.0f));
}

template <typename T>
[[nodiscard]] inline T damp(const T current, const T target, const float response,
                            const float dt) {
  return current + (target - current) * dampingFactor(response, dt);
}

struct Spring1D {
  float value = 0.0f;
  float velocity = 0.0f;
};

[[nodiscard]] inline Spring1D criticallyDampedSpring(const Spring1D state, const float target,
                                                     const float frequency_hz, const float dt) {
  const float omega = std::max(frequency_hz, 0.0f) * tau();
  const float step = std::max(dt, 0.0f);
  const float offset = state.value - target;
  const float decay = std::exp(-omega * step);
  const float value = target + (offset * (1.0f + omega * step) + state.velocity * step) * decay;
  const float velocity =
      (state.velocity * (1.0f - omega * step) - offset * omega * omega * step) * decay;
  return {value, velocity};
}

} // namespace aster
