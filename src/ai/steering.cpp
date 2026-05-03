// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ai/steering.hpp"

#include <algorithm>
#include <cmath>

namespace aster {
namespace {

bool withinRadius(const Vec3 delta, const float radius) {
  return dot(delta, delta) <= radius * radius;
}

} // namespace

Vec3 limitVector(const Vec3 value, const float max_length) {
  const float safe_max = std::max(max_length, 0.0f);
  const float current = length(value);
  if (current <= safe_max || current <= 0.000001f) {
    return value;
  }
  return value * (safe_max / current);
}

Vec3 steerSeek(const SteeringAgent &agent, const Vec3 target) {
  const Vec3 desired = normalize(target - agent.position) * std::max(agent.max_speed, 0.0f);
  return limitVector(desired - agent.velocity, agent.max_force);
}

Vec3 steerFlee(const SteeringAgent &agent, const Vec3 threat) {
  const Vec3 desired = normalize(agent.position - threat) * std::max(agent.max_speed, 0.0f);
  return limitVector(desired - agent.velocity, agent.max_force);
}

Vec3 steerArrive(const SteeringAgent &agent, const Vec3 target, const float slowing_radius) {
  const Vec3 offset = target - agent.position;
  const float distance = length(offset);
  if (distance <= 0.000001f) {
    return limitVector(agent.velocity * -1.0f, agent.max_force);
  }
  const float radius = std::max(slowing_radius, 0.0001f);
  const float speed = std::max(agent.max_speed, 0.0f) * std::clamp(distance / radius, 0.0f, 1.0f);
  const Vec3 desired = offset * (speed / distance);
  return limitVector(desired - agent.velocity, agent.max_force);
}

Vec3 steerContainBox(const SteeringAgent &agent, const Vec3 center, const Vec3 half_extents,
                     const float margin) {
  const Vec3 local = agent.position - center;
  const Vec3 inner{std::max(half_extents.x - margin, 0.0f), std::max(half_extents.y - margin, 0.0f),
                   std::max(half_extents.z - margin, 0.0f)};
  Vec3 target = agent.position;
  bool needs_correction = false;
  const auto correctAxis = [&](const float local_value, const float inner_extent,
                               const float center_value, float &target_axis) {
    if (local_value > inner_extent) {
      target_axis = center_value + inner_extent;
      needs_correction = true;
    } else if (local_value < -inner_extent) {
      target_axis = center_value - inner_extent;
      needs_correction = true;
    }
  };
  correctAxis(local.x, inner.x, center.x, target.x);
  correctAxis(local.y, inner.y, center.y, target.y);
  correctAxis(local.z, inner.z, center.z, target.z);
  return needs_correction ? steerArrive(agent, target, std::max(margin, 0.10f)) : Vec3{};
}

Vec3 steerFlock(const SteeringAgent &agent, const std::vector<SteeringAgent> &neighbors,
                const FlockSteeringSettings &settings) {
  Vec3 separation{};
  Vec3 alignment{};
  Vec3 cohesion{};
  int separation_count = 0;
  int alignment_count = 0;
  int cohesion_count = 0;

  for (const SteeringAgent &neighbor : neighbors) {
    const Vec3 delta = neighbor.position - agent.position;
    const float distance = length(delta);
    if (distance <= 0.0001f) {
      continue;
    }
    if (withinRadius(delta, std::max(settings.separation_radius, 0.0f))) {
      separation = separation - delta / (distance * distance);
      ++separation_count;
    }
    if (withinRadius(delta, std::max(settings.alignment_radius, 0.0f))) {
      alignment = alignment + neighbor.velocity;
      ++alignment_count;
    }
    if (withinRadius(delta, std::max(settings.cohesion_radius, 0.0f))) {
      cohesion = cohesion + neighbor.position;
      ++cohesion_count;
    }
  }

  Vec3 force{};
  if (separation_count > 0) {
    const Vec3 desired = normalize(separation) * std::max(agent.max_speed, 0.0f);
    force =
        force + limitVector(desired - agent.velocity, agent.max_force) * settings.separation_weight;
  }
  if (alignment_count > 0) {
    const Vec3 desired = normalize(alignment / static_cast<float>(alignment_count)) *
                         std::max(agent.max_speed, 0.0f);
    force =
        force + limitVector(desired - agent.velocity, agent.max_force) * settings.alignment_weight;
  }
  if (cohesion_count > 0) {
    const Vec3 center = cohesion / static_cast<float>(cohesion_count);
    force = force + steerArrive(agent, center, std::max(settings.cohesion_radius, 0.001f)) *
                        settings.cohesion_weight;
  }
  return limitVector(force, agent.max_force);
}

void integrateSteering(SteeringAgent &agent, const Vec3 steering_force, const float dt) {
  const float step = std::max(dt, 0.0f);
  agent.velocity = limitVector(agent.velocity + limitVector(steering_force, agent.max_force) * step,
                               agent.max_speed);
  agent.position = agent.position + agent.velocity * step;
}

} // namespace aster
