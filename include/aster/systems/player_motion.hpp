// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"
#include "aster/physics/physics_world.hpp"

namespace aster {

struct PlayerMovementProfile {
  float walk_speed = 3.0f;
  float run_multiplier = 1.65f;
  float response_rate = 11.0f;
  float air_control_ratio = 0.92f;
  float braking_ratio = 2.6f;
  float ground_acceleration_ratio = 3.2f;
  float ground_probe_distance = 0.18f;
  float max_slope_angle = 0.91f;
  float jump_speed = 4.6f;
};

struct PlayerMoveIntent {
  Vec2 move_axis{};
  bool run_requested = false;
  bool jump_requested = false;
};

struct PlayerMovePlan {
  CharacterMoveInput input{};
  CharacterControllerSettings character_settings{};
  float target_speed = 0.0f;
};

[[nodiscard]] PlayerMovePlan buildPlayerMovePlan(const PlayerMovementProfile &profile,
                                                 PlayerMoveIntent intent);

} // namespace aster
