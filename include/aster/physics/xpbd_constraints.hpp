// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <vector>

namespace aster {

struct XpbdParticle {
  Vec3 position{};
  Vec3 previous_position{};
  Vec3 velocity{};
  float inverse_mass = 1.0f;
  bool pinned = false;
};

struct XpbdDistanceConstraint {
  std::size_t a = 0u;
  std::size_t b = 0u;
  float rest_length = 1.0f;
  float compliance = 0.0f;
  float damping = 0.0f;
};

struct XpbdSimulationSettings {
  float dt = 1.0f / 60.0f;
  int iterations = 6;
  Vec3 gravity{0.0f, -9.81f, 0.0f};
};

struct XpbdSimulationReport {
  std::size_t constraints_solved = 0u;
  float max_distance_error = 0.0f;
  bool stable = true;
};

[[nodiscard]] XpbdSimulationReport simulateXpbdDistanceConstraints(
    std::vector<XpbdParticle> &particles, const std::vector<XpbdDistanceConstraint> &constraints,
    XpbdSimulationSettings settings = {});

} // namespace aster
