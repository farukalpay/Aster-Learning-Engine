// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/fixed_timestep.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aster {

FixedTimestep::FixedTimestep(FixedTimestepConfig config) : config_(config) {
  if (config_.step_seconds <= 0.0) {
    throw std::invalid_argument("Fixed timestep requires a positive step duration.");
  }
  if (config_.max_frame_seconds < 0.0) {
    throw std::invalid_argument("Fixed timestep frame clamp cannot be negative.");
  }
  if (config_.max_steps_per_frame == 0u) {
    throw std::invalid_argument("Fixed timestep requires at least one step per frame.");
  }
}

std::size_t FixedTimestep::advance(const double frame_seconds) {
  const double clamped_frame =
      std::clamp(frame_seconds, 0.0, std::max(config_.max_frame_seconds, config_.step_seconds));
  accumulator_seconds_ += clamped_frame;

  std::size_t steps = 0;
  while (accumulator_seconds_ >= config_.step_seconds && steps < config_.max_steps_per_frame) {
    accumulator_seconds_ -= config_.step_seconds;
    ++steps;
  }

  if (steps == config_.max_steps_per_frame && accumulator_seconds_ >= config_.step_seconds) {
    accumulator_seconds_ = std::fmod(accumulator_seconds_, config_.step_seconds);
  }

  return steps;
}

double FixedTimestep::stepSeconds() const {
  return config_.step_seconds;
}

double FixedTimestep::accumulatorSeconds() const {
  return accumulator_seconds_;
}

double FixedTimestep::interpolationAlpha() const {
  return std::clamp(accumulator_seconds_ / config_.step_seconds, 0.0, 1.0);
}

void FixedTimestep::reset() {
  accumulator_seconds_ = 0.0;
}

} // namespace aster
