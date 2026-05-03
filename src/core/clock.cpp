// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/clock.hpp"

#include <chrono>

namespace aster {
namespace {

using SteadyClock = std::chrono::steady_clock;

double secondsSinceStart() {
  static const auto start = SteadyClock::now();
  const std::chrono::duration<double> elapsed = SteadyClock::now() - start;
  return elapsed.count();
}

} // namespace

Clock::Clock() : previous_seconds_(now()) {}

double Clock::tick() {
  const double current = now();
  const double delta = current - previous_seconds_;
  previous_seconds_ = current;
  return delta;
}

double Clock::now() const {
  return secondsSinceStart();
}

} // namespace aster
