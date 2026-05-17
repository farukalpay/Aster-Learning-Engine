// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <cstddef>
#include <vector>

namespace aster {

struct Particle {
  Vec3 position{};
  Vec3 velocity{};
  Vec3 tint{1.0f, 0.55f, 0.18f};
  float age = 0.0f;
  float lifetime = 1.0f;
  float size = 0.05f;
  bool active = false;
};

struct ParticleEmitterDesc {
  std::size_t max_particles = 8u;
  float lifetime = 0.45f;
  float rise_speed = 0.42f;
  float swirl_radius = 0.035f;
  float base_size = 0.045f;
  Vec3 hot_tint{1.0f, 0.47f, 0.10f};
  Vec3 cool_tint{0.45f, 0.055f, 0.018f};
};

class ParticleEmitter {
public:
  explicit ParticleEmitter(std::size_t max_particles = 0u);

  void configure(std::size_t max_particles);
  void reset();
  void update(float dt, Vec3 origin, float seconds, const ParticleEmitterDesc &desc);

  [[nodiscard]] const std::vector<Particle> &particles() const;

private:
  void respawn(std::size_t index, Vec3 origin, float seconds, const ParticleEmitterDesc &desc);

  std::vector<Particle> particles_;
};

} // namespace aster
