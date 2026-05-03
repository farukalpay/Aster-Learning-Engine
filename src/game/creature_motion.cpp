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

} // namespace aster
