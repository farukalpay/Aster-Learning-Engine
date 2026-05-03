// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <vector>

namespace aster {

struct SteeringAgent {
  Vec3 position{};
  Vec3 velocity{};
  float max_speed = 1.0f;
  float max_force = 1.0f;
};

struct FlockSteeringSettings {
  float separation_radius = 1.0f;
  float alignment_radius = 2.0f;
  float cohesion_radius = 2.5f;
  float separation_weight = 1.0f;
  float alignment_weight = 0.6f;
  float cohesion_weight = 0.5f;
};

[[nodiscard]] Vec3 limitVector(Vec3 value, float max_length);
[[nodiscard]] Vec3 steerSeek(const SteeringAgent &agent, Vec3 target);
[[nodiscard]] Vec3 steerFlee(const SteeringAgent &agent, Vec3 threat);
[[nodiscard]] Vec3 steerArrive(const SteeringAgent &agent, Vec3 target, float slowing_radius);
[[nodiscard]] Vec3 steerContainBox(const SteeringAgent &agent, Vec3 center, Vec3 half_extents,
                                   float margin);
[[nodiscard]] Vec3 steerFlock(const SteeringAgent &agent,
                              const std::vector<SteeringAgent> &neighbors,
                              const FlockSteeringSettings &settings);
void integrateSteering(SteeringAgent &agent, Vec3 steering_force, float dt);

} // namespace aster
