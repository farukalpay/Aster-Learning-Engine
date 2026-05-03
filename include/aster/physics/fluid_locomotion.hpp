// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/physics/physics_world.hpp"

namespace aster {

struct SwimMotionSettings {
  float activation_submersion = 0.28f;
  float full_swim_submersion = 0.68f;
  float horizontal_speed_scale = 0.58f;
  float flow_influence = 0.45f;
  float surface_clearance = 0.12f;
  float float_response = 7.0f;
  float max_upward_speed = 1.05f;
  float max_downward_speed = 0.65f;
  float ascend_speed = 1.20f;
};

struct SwimMotionInput {
  Vec3 desired_velocity{};
  bool ascend_requested = false;
};

struct SwimMotionResult {
  bool swimming = false;
  float blend = 0.0f;
  Vec3 desired_velocity{};
  float target_vertical_velocity = 0.0f;
};

[[nodiscard]] SwimMotionResult buildSwimMotion(const PhysicsFluidSample &fluid,
                                               const Vec3 &current_velocity,
                                               const SwimMotionInput &input,
                                               SwimMotionSettings settings = {});

} // namespace aster
