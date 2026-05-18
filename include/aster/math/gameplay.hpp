// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/authoring.hpp"
#include "aster/math/easing.hpp"

namespace aster {

struct MovementMathSettings {
  float acceleration = 18.0f;
  float friction = 10.0f;
  float max_speed = 4.0f;
  float air_control = 0.35f;
};

[[nodiscard]] inline Vec3 projectVelocityOnPlane(const Vec3 velocity, const Vec3 normal) {
  const Vec3 n = normalizeOr(normal, {0.0f, 1.0f, 0.0f});
  return velocity - n * dot(velocity, n);
}

[[nodiscard]] inline Vec3 updatePlanarVelocity(const Vec3 velocity, const Vec3 desired_direction,
                                               const MovementMathSettings settings,
                                               const bool grounded, const float dt) {
  const Vec3 desired =
      normalizeOr(Vec3{desired_direction.x, 0.0f, desired_direction.z}, {0.0f, 0.0f, 0.0f}) *
      std::max(settings.max_speed, 0.0f);
  const float control = grounded ? 1.0f : clamp(settings.air_control, 0.0f, 1.0f);
  Vec3 planar{velocity.x, 0.0f, velocity.z};
  planar = damp(planar, desired, std::max(settings.acceleration, 0.0f) * control, dt);
  if (lengthSquared(desired) <= 0.000001f) {
    planar = damp(planar, Vec3{}, settings.friction, dt);
  }
  return {planar.x, velocity.y, planar.z};
}

struct CameraShake {
  float trauma = 0.0f;
  float decay = 1.5f;
  float amplitude = 1.0f;
};

[[nodiscard]] inline CameraShake updateCameraShake(CameraShake shake, const float dt) {
  shake.trauma = std::max(0.0f, shake.trauma - std::max(shake.decay, 0.0f) * std::max(dt, 0.0f));
  return shake;
}

[[nodiscard]] inline float cameraShakeAmount(const CameraShake shake) {
  return shake.amplitude * shake.trauma * shake.trauma;
}

struct InteractionCone {
  Vec3 origin{};
  Vec3 direction{0.0f, 0.0f, -1.0f};
  float max_distance = 3.0f;
  float min_dot = 0.82f;
};

[[nodiscard]] inline float interactionScore(const InteractionCone cone, const Vec3 target,
                                            const float radius = 0.0f) {
  const Vec3 to_target = target - cone.origin;
  const float distance_value = length(to_target);
  if (distance_value > cone.max_distance + radius || distance_value <= 0.000001f) {
    return 0.0f;
  }
  const float facing = dot(normalizeOr(cone.direction, {0.0f, 0.0f, -1.0f}), to_target / distance_value);
  if (facing < cone.min_dot) {
    return 0.0f;
  }
  return facing * (1.0f - clamp((distance_value - radius) / std::max(cone.max_distance, 0.000001f),
                                0.0f, 1.0f));
}

struct BallisticSolution {
  bool valid = false;
  Vec3 initial_velocity{};
  float time = 0.0f;
};

[[nodiscard]] inline BallisticSolution solveBallisticArc(const Vec3 origin, const Vec3 target,
                                                         const float speed,
                                                         const float gravity = 9.81f) {
  const Vec3 delta = target - origin;
  const Vec2 planar{delta.x, delta.z};
  const float x = length(planar);
  const float y = delta.y;
  const float speed_sq = speed * speed;
  const float discriminant =
      speed_sq * speed_sq - gravity * (gravity * x * x + 2.0f * y * speed_sq);
  if (speed <= 0.0f || gravity <= 0.0f || discriminant < 0.0f || x <= 0.000001f) {
    return {};
  }
  const float angle = std::atan((speed_sq - std::sqrt(discriminant)) / (gravity * x));
  const float time = x / (std::cos(angle) * speed);
  const Vec3 planar_dir = normalizeOr(Vec3{delta.x, 0.0f, delta.z}, {0.0f, 0.0f, 1.0f});
  return {.valid = true,
          .initial_velocity = planar_dir * (std::cos(angle) * speed) +
                              Vec3{0.0f, std::sin(angle) * speed, 0.0f},
          .time = time};
}

[[nodiscard]] inline float damageFalloff(const float distance_value, const float full_damage_range,
                                         const float zero_damage_range) {
  if (distance_value <= full_damage_range) {
    return 1.0f;
  }
  if (distance_value >= zero_damage_range) {
    return 0.0f;
  }
  return 1.0f - smoothstep(full_damage_range, zero_damage_range, distance_value);
}

} // namespace aster
