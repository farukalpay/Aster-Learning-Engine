// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

namespace aster {

struct ClimbableCylinder {
  Vec3 base{};
  float radius = 0.5f;
  float height = 2.0f;
};

struct ClimbSurfaceSample {
  bool climbable = false;
  Vec3 nearest_point{};
  Vec3 outward_normal{1.0f, 0.0f, 0.0f};
  float height_fraction = 0.0f;
  float clearance = 0.0f;
};

struct ClimbMotionSettings {
  float capture_distance = 0.34f;
  float character_clearance = 0.16f;
  float stick_response = 11.0f;
  float tangent_speed_scale = 0.44f;
  float ascend_speed = 1.45f;
  float hold_vertical_speed = 0.18f;
  float max_correction = 0.18f;
};

struct ClimbMotionInput {
  Vec3 desired_velocity{};
  bool engage_requested = false;
  bool ascend_requested = false;
};

struct ClimbMotionResult {
  bool climbing = false;
  float blend = 0.0f;
  Vec3 desired_velocity{};
  Vec3 position_correction{};
};

[[nodiscard]] ClimbSurfaceSample sampleClimbableCylinder(const ClimbableCylinder &cylinder,
                                                         Vec3 character_position,
                                                         ClimbMotionSettings settings = {});
[[nodiscard]] ClimbMotionResult buildClimbMotion(const ClimbSurfaceSample &surface,
                                                 const ClimbMotionInput &input,
                                                 ClimbMotionSettings settings = {});

} // namespace aster
