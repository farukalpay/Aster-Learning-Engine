// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/transition_wipe.hpp"

#include "aster/core/deterministic_sim.hpp"

#include <algorithm>

namespace aster {

void TransitionWipe::start(const int columns, const int height, const std::uint32_t seed,
                           const float seconds) {
  columns_ = std::max(columns, 1);
  height_ = std::max(height, 1);
  seconds_ = std::max(seconds, 0.001f);
  age_ = 0.0f;
  active_ = true;
  offsets_.assign(static_cast<std::size_t>(columns_), 0.0f);

  DeterministicRandomStream random(seed);
  float walk = -random.nextUnit() * 0.16f;
  for (float &offset : offsets_) {
    walk = std::clamp(walk + random.nextSigned() * 0.045f, -0.22f, 0.08f);
    offset = walk;
  }
}

void TransitionWipe::update(const float dt) {
  if (!active_) {
    return;
  }
  age_ += std::max(dt, 0.0f);
  if (age_ >= seconds_ * 1.18f) {
    active_ = false;
  }
}

void TransitionWipe::finish() {
  active_ = false;
  age_ = seconds_;
}

bool TransitionWipe::active() const {
  return active_;
}

bool TransitionWipe::finished() const {
  return !active_ && age_ >= seconds_;
}

TransitionWipeFrame TransitionWipe::frame() const {
  TransitionWipeFrame out;
  out.active = active_;
  out.finished = finished();
  out.width = columns_;
  out.height = height_;
  out.column_progress.reserve(offsets_.size());
  for (const float offset : offsets_) {
    out.column_progress.push_back(std::clamp(age_ / seconds_ + offset, 0.0f, 1.0f));
  }
  return out;
}

} // namespace aster
