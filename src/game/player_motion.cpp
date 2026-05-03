// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game/player_motion.hpp"

#include <algorithm>

namespace aster {

PlayerMovePlan buildPlayerMovePlan(const PlayerMovementProfile &profile, PlayerMoveIntent intent) {
  if (length(intent.move_axis) > 1.0f) {
    intent.move_axis = normalize(intent.move_axis);
  }

  const float run_multiplier = std::max(profile.run_multiplier, 1.0f);
  const float speed =
      std::max(profile.walk_speed, 0.0f) * (intent.run_requested ? run_multiplier : 1.0f);

  PlayerMovePlan plan;
  plan.target_speed = speed;
  plan.input.desired_velocity = {intent.move_axis.x * speed, 0.0f, intent.move_axis.y * speed};
  plan.input.jump_requested = intent.jump_requested;
  plan.character_settings.acceleration =
      std::max(profile.response_rate * profile.ground_acceleration_ratio, 0.0f);
  plan.character_settings.air_acceleration =
      std::max(profile.response_rate * profile.air_control_ratio, 0.0f);
  plan.character_settings.braking = std::max(profile.response_rate * profile.braking_ratio, 0.0f);
  plan.character_settings.ground_probe_distance = std::max(profile.ground_probe_distance, 0.0f);
  plan.character_settings.max_slope_angle = std::max(profile.max_slope_angle, 0.0f);
  plan.character_settings.jump_speed = std::max(profile.jump_speed, 0.0f);
  return plan;
}

} // namespace aster
