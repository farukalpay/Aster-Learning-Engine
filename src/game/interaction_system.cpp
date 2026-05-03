// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/interaction_system.hpp"

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
  ray_direction = normalize(ray_direction);
  const float previous_strength = focus_.strength;

  const InteractionTarget *best = nullptr;
  float best_distance = std::numeric_limits<float>::max();
  for (const InteractionTarget &target : targets) {
    if (!target.enabled || target.id.empty()) {
      continue;
    }
    float distance = 0.0f;
    if (!rayIntersectsTarget(ray_origin, ray_direction, target, distance)) {
      continue;
    }
    if (distance < best_distance) {
      best = &target;
      best_distance = distance;
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
  focus_.kind = best->kind;
  focus_.action_label = best->action_label;
  focus_.subject_label = best->subject_label;
  focus_.position = best->position;
  focus_.distance = best_distance;
  focus_.strength = approach(same_target ? previous_strength : 0.0f, 1.0f, 16.0f, dt);
  focus_.visible = true;
}

const InteractionFocus &InteractionSystem::focus() const {
  return focus_;
}

} // namespace aster
