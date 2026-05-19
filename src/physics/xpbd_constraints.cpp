// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/physics/xpbd_constraints.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

XpbdSimulationReport simulateXpbdDistanceConstraints(
    std::vector<XpbdParticle> &particles, const std::vector<XpbdDistanceConstraint> &constraints,
    XpbdSimulationSettings settings) {
  XpbdSimulationReport report;
  if (particles.empty() || constraints.empty() || settings.dt <= 0.0f || settings.iterations <= 0) {
    report.stable = particles.empty() || constraints.empty();
    return report;
  }

  const float dt = settings.dt;
  for (XpbdParticle &particle : particles) {
    particle.previous_position = particle.position;
    if (!particle.pinned && particle.inverse_mass > 0.0f) {
      particle.velocity += settings.gravity * dt;
      particle.position += particle.velocity * dt;
    }
  }

  std::vector<float> lambdas(constraints.size(), 0.0f);
  for (int iteration = 0; iteration < settings.iterations; ++iteration) {
    for (std::size_t i = 0u; i < constraints.size(); ++i) {
      const XpbdDistanceConstraint &constraint = constraints[i];
      if (constraint.a >= particles.size() || constraint.b >= particles.size()) {
        report.stable = false;
        continue;
      }
      XpbdParticle &a = particles[constraint.a];
      XpbdParticle &b = particles[constraint.b];
      const float inv_a = a.pinned ? 0.0f : a.inverse_mass;
      const float inv_b = b.pinned ? 0.0f : b.inverse_mass;
      const float inverse_mass_sum = inv_a + inv_b;
      if (inverse_mass_sum <= 0.0f) {
        continue;
      }
      const Vec3 delta = b.position - a.position;
      const float distance = length(delta);
      if (distance <= 0.000001f) {
        continue;
      }
      const Vec3 direction = delta / distance;
      const float error = distance - constraint.rest_length;
      const float alpha = constraint.compliance / (dt * dt);
      const float damping = std::clamp(constraint.damping, 0.0f, 1.0f);
      const float dlambda = (-error - alpha * lambdas[i]) / (inverse_mass_sum + alpha);
      lambdas[i] += dlambda;
      const Vec3 correction = direction * dlambda * (1.0f - damping);
      if (inv_a > 0.0f) {
        a.position -= correction * inv_a;
      }
      if (inv_b > 0.0f) {
        b.position += correction * inv_b;
      }
      report.max_distance_error = std::max(report.max_distance_error, std::abs(error));
      ++report.constraints_solved;
    }
  }

  for (XpbdParticle &particle : particles) {
    if (!particle.pinned) {
      particle.velocity = (particle.position - particle.previous_position) / dt;
    } else {
      particle.velocity = {};
    }
  }
  return report;
}

} // namespace aster
