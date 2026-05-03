// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/particle_system.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float phaseFor(const std::size_t index, const float seconds) {
  return static_cast<float>(index) * 2.399963f + seconds * 0.73f;
}

Vec3 mix(const Vec3 a, const Vec3 b, const float t) {
  return a * (1.0f - t) + b * t;
}

} // namespace

ParticleEmitter::ParticleEmitter(const std::size_t max_particles) : particles_(max_particles) {}

void ParticleEmitter::configure(const std::size_t max_particles) {
  particles_.resize(max_particles);
}

void ParticleEmitter::reset() {
  for (Particle &particle : particles_) {
    particle = {};
  }
}

void ParticleEmitter::update(const float dt, const Vec3 origin, const float seconds,
                             const ParticleEmitterDesc &desc) {
  if (particles_.size() != desc.max_particles) {
    configure(desc.max_particles);
  }
  for (std::size_t i = 0; i < particles_.size(); ++i) {
    Particle &particle = particles_[i];
    if (!particle.active || particle.age >= particle.lifetime) {
      respawn(i, origin, seconds, desc);
      continue;
    }
    particle.age += std::max(0.0f, dt);
    const float t = std::clamp(particle.age / std::max(particle.lifetime, 0.001f), 0.0f, 1.0f);
    const float swirl = phaseFor(i, seconds) + t * kPi * 1.5f;
    particle.position =
        particle.position + particle.velocity * std::max(0.0f, dt) +
        Vec3{std::cos(swirl), 0.0f, std::sin(swirl)} * (desc.swirl_radius * 0.16f * (1.0f - t));
    particle.tint = mix(desc.hot_tint, desc.cool_tint, t);
    particle.size = desc.base_size * (1.0f - t * 0.72f);
  }
}

const std::vector<Particle> &ParticleEmitter::particles() const {
  return particles_;
}

void ParticleEmitter::respawn(const std::size_t index, const Vec3 origin, const float seconds,
                              const ParticleEmitterDesc &desc) {
  const float phase = phaseFor(index, seconds);
  const float ring = 0.35f + 0.65f * std::abs(std::sin(phase * 1.31f));
  Particle &particle = particles_[index];
  particle.position =
      origin + Vec3{std::cos(phase), 0.0f, std::sin(phase)} * (desc.swirl_radius * ring);
  particle.velocity = {std::sin(phase * 1.7f) * 0.035f, desc.rise_speed,
                       std::cos(phase * 1.4f) * 0.035f};
  particle.tint = desc.hot_tint;
  particle.age = std::fmod(std::abs(std::sin(phase)) * desc.lifetime, desc.lifetime * 0.35f);
  particle.lifetime = std::max(desc.lifetime, 0.001f);
  particle.size = desc.base_size;
  particle.active = true;
}

} // namespace aster
