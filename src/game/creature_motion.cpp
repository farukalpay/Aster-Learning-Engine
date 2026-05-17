// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/creature_motion.hpp"

#include "aster/ai/behavior_tree.hpp"
#include "aster/ai/steering.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

aster::Vec2 planar(const aster::Vec3 value) {
  return {value.x, value.z};
}

aster::Vec3 fromPlanar(const aster::Vec2 value, const float y) {
  return {value.x, y, value.y};
}

float ellipseDistance(const aster::Vec2 point, const aster::Vec2 radius) {
  const float rx = std::max(radius.x, 0.001f);
  const float rz = std::max(radius.y, 0.001f);
  return std::sqrt((point.x * point.x) / (rx * rx) + (point.y * point.y) / (rz * rz));
}

aster::Vec2 clampToEllipse(const aster::Vec2 point, const aster::Vec2 radius,
                           const float normalized_radius) {
  const float distance = ellipseDistance(point, radius);
  if (distance <= std::max(normalized_radius, 0.001f)) {
    return point;
  }
  return point * (std::max(normalized_radius, 0.001f) / std::max(distance, 0.001f));
}

float yawTo(const aster::Vec3 from, const aster::Vec3 to) {
  const aster::Vec3 delta = to - from;
  return std::atan2(delta.x, delta.z);
}

aster::Vec3 approach(const aster::Vec3 current, const aster::Vec3 target, const float max_delta) {
  const aster::Vec3 delta = target - current;
  const float distance = aster::length(delta);
  if (distance <= max_delta || distance <= 0.0001f) {
    return target;
  }
  return current + delta * (max_delta / distance);
}

float shorelineLerp(float phase) {
  return 0.5f + 0.5f * std::sin(phase);
}

aster::Vec3 wanderVector(const float phase, const float temperament) {
  return aster::normalize({std::sin(phase * 1.73f + temperament * 4.1f) * 0.76f,
                           std::sin(phase * 0.91f + temperament * 2.7f) * 0.28f,
                           std::cos(phase * 1.29f + temperament * 5.3f) * 0.76f});
}

float signedAngleToYaw(const aster::Vec3 value) {
  return std::atan2(value.x, value.z);
}

} // namespace

namespace aster {

AmphibiousPredatorUpdate updateAmphibiousPredator(AmphibiousPredatorState &state,
                                                  const AmphibiousPredatorSettings &settings,
                                                  const Vec3 target_position, const float dt) {
  const float step = std::max(dt, 0.0f);
  state.phase += step;
  state.strike_cooldown = std::max(0.0f, state.strike_cooldown - step);

  const Vec2 center{settings.water_center.x, settings.water_center.z};
  const Vec2 target_local = planar(target_position) - center;
  const float target_distance = ellipseDistance(target_local, settings.water_radius);
  const float world_target_distance = length(target_position - state.position);
  const float aggression = std::clamp(settings.aggression, 0.0f, 1.0f);
  const float notice_radius = std::max(settings.notice_radius * (0.65f + aggression * 0.70f), 0.0f);

  Vec3 desired = state.position;
  float speed = std::max(settings.swim_speed, 0.0f);
  float swim_blend = 1.0f;
  bool strike = false;
  const float swim_submergence = std::clamp(settings.swim_submergence, 0.0f, 1.0f);
  const float swim_y = settings.water_surface_y - settings.body_height * swim_submergence;

  (void)tickSelector(
      {behaviorAction("pursue target in water",
                      [&] {
                        if (world_target_distance > notice_radius ||
                            target_distance >
                                1.0f + std::max(settings.water_pursuit_margin, 0.0f)) {
                          return BehaviorStatus::Failure;
                        }
                        const Vec2 chase_local =
                            clampToEllipse(target_local, settings.water_radius, 0.92f);
                        desired = fromPlanar(center + chase_local, swim_y);
                        state.mode = AmphibiousMotionMode::Pursue;
                        speed = std::max(settings.pursue_speed, 0.0f);
                        if (world_target_distance <= std::max(settings.strike_radius, 0.0f) &&
                            state.strike_cooldown <= 0.0f) {
                          strike = true;
                          state.strike_cooldown = std::max(settings.strike_cooldown, 0.0f);
                          state.mode = AmphibiousMotionMode::Strike;
                        }
                        return BehaviorStatus::Success;
                      }),
       behaviorAction("patrol water edge", [&] {
         const float shore_mix = shorelineLerp(state.phase * 0.23f);
         const float angle = state.phase * (0.42f + aggression * 0.18f);
         const float patrol_min_radius = std::clamp(settings.patrol_min_radius, 0.0f, 1.50f);
         const float patrol_max_radius =
             std::max(patrol_min_radius, std::clamp(settings.patrol_max_radius, 0.0f, 1.50f));
         const float shore_radius = std::max(settings.shore_radius, 0.0f);
         const float normalized_radius =
             patrol_min_radius + shore_mix * (patrol_max_radius - patrol_min_radius);
         const Vec2 patrol_local{std::cos(angle) * settings.water_radius.x * normalized_radius,
                                 std::sin(angle * 0.86f + kPi * 0.18f) * settings.water_radius.y *
                                     normalized_radius};
         const bool on_shore = normalized_radius > shore_radius;
         desired = fromPlanar(center + patrol_local,
                              settings.water_surface_y +
                                  (on_shore ? settings.body_height * 0.28f
                                            : -settings.body_height * swim_submergence));
         state.mode = on_shore ? AmphibiousMotionMode::Shore : AmphibiousMotionMode::Swim;
         speed =
             on_shore ? std::max(settings.shore_speed, 0.0f) : std::max(settings.swim_speed, 0.0f);
         swim_blend = on_shore ? 0.12f : 1.0f;
         return BehaviorStatus::Success;
       })});

  state.position = approach(state.position, desired, speed * step);
  if (length(desired - state.position) > 0.0001f) {
    state.facing_yaw = yawTo(state.position, desired);
  }

  return {.strike = strike,
          .position = state.position,
          .facing_yaw = state.facing_yaw,
          .swim_blend = swim_blend,
          .mode = state.mode};
}

AvianAgentUpdate updateAvianAgent(AvianAgentState &state, const AvianFlockSettings &settings,
                                  const std::vector<AvianAgentState> &flock,
                                  const Vec3 disturbance_position, const float dt) {
  const float step = std::max(dt, 0.0f);
  state.wing_phase += step * (7.4f + state.temperament * 3.1f);

  SteeringAgent agent{state.position, state.velocity, std::max(settings.max_speed, 0.0f),
                      std::max(settings.max_force, 0.0f)};
  std::vector<SteeringAgent> neighbors;
  neighbors.reserve(flock.size());
  for (const AvianAgentState &other : flock) {
    neighbors.push_back({other.position, other.velocity, agent.max_speed, agent.max_force});
  }

  Vec3 steering{};
  const float disturbance_distance = length(disturbance_position - state.position);
  const float home_distance = length(state.home_position - state.position);
  const float nest_distance = length(settings.nest_position - state.position);

  (void)tickSelector(
      {behaviorAction("flee disturbance",
                      [&] {
                        if (disturbance_distance > std::max(settings.fear_radius, 0.0f)) {
                          return BehaviorStatus::Failure;
                        }
                        const Vec3 elevated_threat =
                            disturbance_position - Vec3{0.0f, settings.perch_height + 0.45f, 0.0f};
                        steering = steering + steerFlee(agent, elevated_threat) * 1.9f;
                        steering = steering + steerContainBox(agent, settings.flight_center,
                                                              settings.flight_half_extents, 0.85f);
                        state.mode = AvianMotionMode::Flee;
                        return BehaviorStatus::Success;
                      }),
       behaviorAction("return to nest volume",
                      [&] {
                        if (home_distance <= std::max(settings.nest_return_radius, 0.0f)) {
                          return BehaviorStatus::Failure;
                        }
                        steering =
                            steering +
                            steerArrive(agent, state.home_position,
                                        std::max(settings.nest_return_radius * 0.35f, 0.15f)) *
                                1.3f;
                        state.mode = AvianMotionMode::ReturnToNest;
                        return BehaviorStatus::Success;
                      }),
       behaviorAction(
           "land near nest",
           [&] {
             const float landing_bias =
                 0.5f + 0.5f * std::sin(state.wing_phase * 0.27f + state.temperament * 6.0f);
             if (disturbance_distance < settings.fear_radius * 1.35f ||
                 nest_distance > std::max(settings.landing_radius, 0.0f) || landing_bias < 0.72f) {
               return BehaviorStatus::Failure;
             }
             const Vec3 perch = settings.nest_position + Vec3{0.0f, settings.perch_height, 0.0f};
             steering = steering + steerArrive(agent, perch, settings.landing_radius) * 1.5f;
             state.mode = AvianMotionMode::Land;
             return BehaviorStatus::Success;
           }),
       behaviorAction("flock and wander", [&] {
         steering = steering + steerFlock(agent, neighbors,
                                          {.separation_radius = 0.86f,
                                           .alignment_radius = 2.4f,
                                           .cohesion_radius = 3.1f,
                                           .separation_weight = 1.25f,
                                           .alignment_weight = 0.52f,
                                           .cohesion_weight = 0.46f});
         steering = steering + wanderVector(state.wing_phase, state.temperament) *
                                   (settings.wander_strength * agent.max_force);
         steering = steering + steerContainBox(agent, settings.flight_center,
                                               settings.flight_half_extents, 0.75f) *
                                   1.6f;
         state.mode = AvianMotionMode::Flock;
         return BehaviorStatus::Success;
       })});

  if (state.mode != AvianMotionMode::Flock) {
    steering = steering +
               steerContainBox(agent, settings.flight_center, settings.flight_half_extents, 0.58f);
  }

  integrateSteering(agent, steering, step);
  state.position = agent.position;
  state.velocity = agent.velocity;
  if (length(state.velocity) > 0.0001f) {
    state.facing_yaw = signedAngleToYaw(state.velocity);
    state.pitch = std::clamp(-state.velocity.y / std::max(agent.max_speed, 0.001f), -0.42f, 0.42f);
    state.roll = std::clamp(-state.velocity.x / std::max(agent.max_speed, 0.001f), -0.55f, 0.55f);
  }

  const float landing_target = state.mode == AvianMotionMode::Land ? 1.0f : 0.0f;
  state.landing_blend = std::clamp(
      state.landing_blend + (landing_target - state.landing_blend) * step * 3.2f, 0.0f, 1.0f);

  return {.position = state.position,
          .velocity = state.velocity,
          .facing_yaw = state.facing_yaw,
          .pitch = state.pitch,
          .roll = state.roll,
          .wing_phase = state.wing_phase,
          .landing_blend = state.landing_blend,
          .mode = state.mode};
}

std::vector<CaveSkitterAgentUpdate>
updateCaveSkitterGroup(std::vector<CaveSkitterAgentState> &skitters,
                       const CaveSkitterGroupSettings &settings, const Vec3 player_position,
                       const float dt) {
  const float step = std::max(dt, 0.0f);
  std::vector<CaveSkitterAgentUpdate> updates;
  updates.reserve(skitters.size());

  Vec3 normal = length(settings.web.normal) > 0.0001f ? normalize(settings.web.normal)
                                                       : Vec3{0.0f, 0.0f, 1.0f};
  Vec3 side = length(settings.web.side) > 0.0001f ? normalize(settings.web.side)
                                                  : Vec3{1.0f, 0.0f, 0.0f};
  side = normalize(side - normal * dot(side, normal));
  if (length(side) <= 0.0001f) {
    side = normalize(cross({0.0f, 1.0f, 0.0f}, normal));
  }
  if (length(side) <= 0.0001f) {
    side = {1.0f, 0.0f, 0.0f};
  }
  Vec3 up = length(settings.web.up) > 0.0001f ? normalize(settings.web.up) : cross(normal, side);
  up = normalize(up - normal * dot(up, normal) - side * dot(up, side));
  if (length(up) <= 0.0001f) {
    up = normalize(cross(normal, side));
  }

  const float radius_x = std::max(settings.web.radius_x, 0.05f);
  const float radius_y = std::max(settings.web.radius_y, 0.05f);
  const float half_thickness = std::max(settings.web.thickness * 0.5f, 0.02f);
  const float depth_wander =
      settings.depth_wander >= 0.0f ? settings.depth_wander : half_thickness * 0.52f;
  const auto localToWorld = [&](const Vec2 local, const float depth) {
    return settings.web.center + side * local.x + up * local.y +
           normal * (settings.surface_offset + depth);
  };
  const auto clampLocal = [&](const Vec2 local, const float inset) {
    const float safe_x = std::max(radius_x * std::max(inset, 0.05f), 0.05f);
    const float safe_y = std::max(radius_y * std::max(inset, 0.05f), 0.05f);
    const float normalized =
        std::sqrt((local.x * local.x) / (safe_x * safe_x) + (local.y * local.y) / (safe_y * safe_y));
    if (normalized <= 1.0f || normalized <= 0.0001f) {
      return local;
    }
    return local / normalized;
  };
  const auto clampWorld = [&](const Vec3 world) {
    const Vec3 offset = world - settings.web.center;
    const Vec2 local = clampLocal({dot(offset, side), dot(offset, up)}, 0.94f);
    const float depth =
        std::clamp(dot(offset, normal) - settings.surface_offset, -depth_wander, depth_wander);
    return localToWorld(local, depth);
  };
  const auto projectedPlayer = [&] {
    const Vec3 offset = player_position - settings.web.center;
    return localToWorld(clampLocal({dot(offset, side), dot(offset, up)}, 0.92f),
                        std::clamp(dot(offset, normal) - settings.surface_offset, -depth_wander,
                                   depth_wander));
  };

  std::vector<SteeringAgent> neighbors;
  neighbors.reserve(skitters.size());
  for (const CaveSkitterAgentState &other : skitters) {
    if (!other.dead) {
      neighbors.push_back(
          {other.position, other.velocity, std::max(settings.max_speed, 0.0f),
           std::max(settings.max_force, 0.0f)});
    }
  }

  for (CaveSkitterAgentState &state : skitters) {
    CaveSkitterAgentUpdate update;
    state.phase += step * (1.0f + state.temperament * 0.24f);
    state.bite_cooldown = std::max(0.0f, state.bite_cooldown - step);
    state.flinch_seconds = std::max(0.0f, state.flinch_seconds - step);

    if (state.dead) {
      state.velocity = {};
      state.mode = CaveSkitterMotionMode::Dead;
      update = {.position = state.position,
                .velocity = state.velocity,
                .facing_yaw = state.facing_yaw,
                .mode = state.mode,
                .bite = false};
      updates.push_back(update);
      continue;
    }

    if (length(state.position) <= 0.0001f) {
      state.position = clampWorld(localToWorld(state.home_offset, 0.0f));
    }

    const Vec3 player_on_web = projectedPlayer();
    const float player_distance = length(player_on_web - state.position);
    const float player_world_distance = length(player_position - state.position);
    const bool aggro =
        player_world_distance <= std::max(settings.aggro_radius, settings.strike_radius);

    Vec3 target = state.position;
    float speed_scale = 1.0f;
    bool bite = false;
    if (state.flinch_seconds > 0.0f) {
      const Vec3 away = normalize(state.position - player_on_web);
      target = clampWorld(state.position + (length(away) > 0.0001f ? away : normal) *
                                             (0.26f + settings.retreat_seconds * 0.12f));
      speed_scale = 0.72f;
      state.mode = CaveSkitterMotionMode::Flinch;
    } else if (aggro && player_distance <= std::max(settings.strike_radius, 0.05f) &&
               state.bite_cooldown <= 0.0f) {
      target = player_on_web;
      bite = true;
      state.bite_cooldown = std::max(settings.bite_cooldown, 0.05f);
      state.mode = CaveSkitterMotionMode::Strike;
    } else if (aggro) {
      target = player_on_web;
      state.mode = CaveSkitterMotionMode::Pursue;
    } else {
      Vec2 home = state.home_offset;
      if (length(home) <= 0.0001f) {
        const float angle = state.temperament * kPi * 2.0f;
        home = {std::cos(angle) * radius_x * 0.46f, std::sin(angle) * radius_y * 0.36f};
      }
      const float orbit = state.phase * (0.58f + state.temperament * 0.18f);
      const Vec2 wander{std::cos(orbit) * radius_x * 0.14f,
                        std::sin(orbit * 1.37f) * radius_y * 0.12f};
      const float depth = std::sin(state.phase * 1.9f + state.temperament * 3.1f) *
                          depth_wander;
      target = localToWorld(clampLocal(home + wander, 0.78f), depth);
      speed_scale = std::max(settings.patrol_speed_scale, 0.05f);
      state.mode = player_world_distance <= settings.aggro_radius * 1.20f
                       ? CaveSkitterMotionMode::Guard
                       : CaveSkitterMotionMode::Patrol;
    }

    SteeringAgent agent{state.position,
                        state.velocity,
                        std::max(settings.max_speed * speed_scale, 0.0f),
                        std::max(settings.max_force, 0.0f)};
    Vec3 steering = steerArrive(agent, target, state.mode == CaveSkitterMotionMode::Pursue ? 0.42f
                                                                                           : 0.28f);
    steering = steering +
               steerFlock(agent, neighbors,
                          {.separation_radius = std::max(settings.separation_radius, 0.01f),
                           .alignment_radius = std::max(settings.cohesion_radius, 0.01f),
                           .cohesion_radius = std::max(settings.cohesion_radius, 0.01f),
                           .separation_weight = 1.45f,
                           .alignment_weight = 0.14f,
                           .cohesion_weight = state.mode == CaveSkitterMotionMode::Pursue ? 0.26f
                                                                                          : 0.12f});
    integrateSteering(agent, steering, step);
    state.position = clampWorld(agent.position);
    state.velocity = agent.velocity;
    if (length(state.velocity) > 0.0001f) {
      state.facing_yaw = signedAngleToYaw(state.velocity);
    } else if (aggro) {
      state.facing_yaw = yawTo(state.position, player_on_web);
    }

    update = {.position = state.position,
              .velocity = state.velocity,
              .facing_yaw = state.facing_yaw,
              .mode = state.mode,
              .bite = bite};
    updates.push_back(update);
  }

  return updates;
}

} // namespace aster
