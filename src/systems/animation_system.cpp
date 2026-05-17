// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/animation_system.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

float easeOutCubic(const float value) {
  const float t = std::clamp(value, 0.0f, 1.0f);
  const float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

void ScalarAnimation::snap(const float value) {
  value_ = value;
  target_ = value;
}

void ScalarAnimation::setTarget(const float target) {
  target_ = target;
}

void ScalarAnimation::update(const float dt) {
  const float t = 1.0f - std::exp(-std::max(response, 0.0f) * std::max(dt, 0.0f));
  value_ += (target_ - value_) * t;
}

float ScalarAnimation::value() const {
  return value_;
}

float ScalarAnimation::target() const {
  return target_;
}

} // namespace aster
