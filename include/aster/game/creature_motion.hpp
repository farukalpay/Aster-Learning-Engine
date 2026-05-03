// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

#include <vector>

namespace aster {

enum class AmphibiousMotionMode {
  Swim,
  Shore,
  Pursue,
  Strike,
};

struct AmphibiousPredatorSettings {
  Vec3 water_center{};
  Vec2 water_radius{1.0f, 1.0f};
  float water_surface_y = 0.0f;
  float body_height = 0.22f;
  float swim_submergence = 0.12f;
  float swim_speed = 0.85f;
  float shore_speed = 0.48f;
  float pursue_speed = 1.05f;
  float aggression = 0.55f;
  float notice_radius = 3.8f;
  float water_pursuit_margin = 0.18f;
  float patrol_min_radius = 0.52f;
  float patrol_max_radius = 1.07f;
  float shore_radius = 0.96f;
  float strike_radius = 0.46f;
  float strike_cooldown = 1.65f;
};

struct AmphibiousPredatorState {
  Vec3 position{};
  float facing_yaw = 0.0f;
  float phase = 0.0f;
  float strike_cooldown = 0.0f;
  AmphibiousMotionMode mode = AmphibiousMotionMode::Swim;
};

struct AmphibiousPredatorUpdate {
  bool strike = false;
  Vec3 position{};
  float facing_yaw = 0.0f;
  float swim_blend = 1.0f;
  AmphibiousMotionMode mode = AmphibiousMotionMode::Swim;
};

enum class AvianMotionMode {
  Flock,
  ReturnToNest,
  Flee,
  Land,
};

struct AvianAgentState {
  Vec3 position{};
  Vec3 velocity{};
  Vec3 home_position{};
  float facing_yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float wing_phase = 0.0f;
  float landing_blend = 0.0f;
  float temperament = 0.5f;
  AvianMotionMode mode = AvianMotionMode::Flock;
};

struct AvianFlockSettings {
  Vec3 flight_center{};
  Vec3 flight_half_extents{1.0f, 1.0f, 1.0f};
  Vec3 nest_position{};
  float max_speed = 2.0f;
  float max_force = 5.0f;
  float fear_radius = 3.0f;
  float nest_return_radius = 6.0f;
  float landing_radius = 0.75f;
  float perch_height = 0.35f;
  float wander_strength = 0.6f;
};

struct AvianAgentUpdate {
  Vec3 position{};
  Vec3 velocity{};
  float facing_yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float wing_phase = 0.0f;
  float landing_blend = 0.0f;
  AvianMotionMode mode = AvianMotionMode::Flock;
};

[[nodiscard]] AmphibiousPredatorUpdate
updateAmphibiousPredator(AmphibiousPredatorState &state, const AmphibiousPredatorSettings &settings,
                         Vec3 target_position, float dt);

[[nodiscard]] AvianAgentUpdate updateAvianAgent(AvianAgentState &state,
                                                const AvianFlockSettings &settings,
                                                const std::vector<AvianAgentState> &flock,
                                                Vec3 disturbance_position, float dt);

} // namespace aster
