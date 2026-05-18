// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/interaction_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aster {

namespace {

float approach(const float value, const float target, const float response, const float dt) {
  const float t = 1.0f - std::exp(-std::max(response, 0.0f) * std::max(dt, 0.0f));
  return value + (target - value) * t;
}

bool rayIntersectsTarget(const Vec3 origin, const Vec3 direction, const InteractionTarget &target,
                         float &distance) {
  if (target.shape == InteractionTargetShape::ExplicitHit) {
    if (target.hit_distance < 0.0f || target.hit_distance > target.max_distance) {
      return false;
    }
    distance = target.hit_distance;
    return true;
  }

  const Vec3 offset = target.position - origin;
  const float projected = dot(offset, direction);
  if (projected < 0.0f || projected > target.max_distance) {
    return false;
  }
  const Vec3 closest = origin + direction * projected;
  const float miss = length(target.position - closest);
  if (miss > target.radius) {
    return false;
  }
  distance = projected;
  return true;
}

} // namespace

void InteractionSystem::clear() {
  focus_ = {};
}

void InteractionSystem::update(const std::vector<InteractionTarget> &targets, const Vec3 ray_origin,
                               Vec3 ray_direction, const float dt) {
  update(targets, ray_origin, ray_direction, ray_origin, dt);
}

void InteractionSystem::update(const std::vector<InteractionTarget> &targets, const Vec3 ray_origin,
                               Vec3 ray_direction, const Vec3 actor_position, const float dt) {
  ray_direction = normalize(ray_direction);
  const float previous_strength = focus_.strength;

  const InteractionTarget *best = nullptr;
  bool best_ray_hit = false;
  float best_distance = std::numeric_limits<float>::max();
  float best_evidence = -std::numeric_limits<float>::infinity();
  for (const InteractionTarget &target : targets) {
    if (!target.enabled || target.id.empty() || target.occluded) {
      continue;
    }
    float distance = 0.0f;
    const bool ray_hit = rayIntersectsTarget(ray_origin, ray_direction, target, distance);
    if (!ray_hit) {
      if (best_ray_hit || target.proximity_distance <= 0.0f) {
        continue;
      }
      distance = length(target.position - actor_position);
      if (distance > target.proximity_distance) {
        continue;
      }
    }

    const float evidence = std::max(target.evidence_strength, 0.0f);
    const bool stronger_evidence =
        ray_hit == best_ray_hit && std::abs(distance - best_distance) <= 0.015f &&
        evidence > best_evidence;
    if ((ray_hit && !best_ray_hit) || stronger_evidence ||
        (ray_hit == best_ray_hit && distance < best_distance - 0.015f)) {
      best = &target;
      best_ray_hit = ray_hit;
      best_distance = distance;
      best_evidence = evidence;
    }
  }

  if (best == nullptr) {
    focus_.strength = approach(previous_strength, 0.0f, 12.0f, dt);
    if (focus_.strength <= 0.025f) {
      focus_ = {};
    } else {
      focus_.visible = true;
    }
    return;
  }

  const bool same_target = focus_.target_id == best->id;
  focus_.target_id = best->id;
  focus_.action_graph = best->action_graph;
  focus_.kind = best->kind;
  focus_.action_label = best->action_label;
  focus_.subject_label = best->subject_label;
  focus_.position = best->position;
  focus_.distance = best_distance;
  focus_.strength = approach(same_target ? previous_strength : 0.0f, 1.0f, 16.0f, dt);
  focus_.user_data = best->user_data;
  focus_.visible = true;
}

const InteractionFocus &InteractionSystem::focus() const {
  return focus_;
}

} // namespace aster
