// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

namespace aster {

struct ThirdPersonFollowSettings {
  float yaw_sensitivity = 0.008f;
  float pitch_sensitivity = 0.006f;
  float min_pitch = radians(10.0f);
  float max_pitch = radians(54.0f);
  float target_response = 18.0f;
  float teleport_snap_distance = 9.0f;
};

struct ThirdPersonFollowInput {
  bool active = false;
  bool has_pointer_delta = false;
  Vec2 pointer_delta{};
  Vec3 focus_target{};
  float fallback_yaw = radians(38.0f);
  float fallback_pitch = radians(28.0f);
};

struct ThirdPersonFollowState {
  bool initialized = false;
  bool active = false;
  float pitch = radians(28.0f);
  float camera_yaw = radians(38.0f);
  Vec3 focus_target{};
};

struct ThirdPersonFollowPose {
  bool active = false;
  float camera_yaw = radians(38.0f);
  float camera_pitch = radians(28.0f);
  Vec3 camera_target{};
};

[[nodiscard]] ThirdPersonFollowPose
updateThirdPersonFollow(ThirdPersonFollowState &state, const ThirdPersonFollowSettings &settings,
                        const ThirdPersonFollowInput &input, float dt);
[[nodiscard]] Vec2 cameraRelativeMoveAxis(Vec2 local_axis, float camera_yaw);

} // namespace aster
