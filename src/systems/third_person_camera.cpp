// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/third_person_camera.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

float wrapRadians(float value) {
  while (value > kPi) {
    value -= kPi * 2.0f;
  }
  while (value < -kPi) {
    value += kPi * 2.0f;
  }
  return value;
}

float dampingFactor(const float response, const float dt) {
  if (response <= 0.0f) {
    return 1.0f;
  }
  return 1.0f - std::exp(-std::max(response, 0.0f) * std::max(dt, 0.0f));
}

aster::Vec3 dampVec3(const aster::Vec3 current, const aster::Vec3 target, const float response,
                     const float dt) {
  return current + (target - current) * dampingFactor(response, dt);
}

} // namespace

namespace aster {

ThirdPersonFollowPose updateThirdPersonFollow(ThirdPersonFollowState &state,
                                              const ThirdPersonFollowSettings &settings,
                                              const ThirdPersonFollowInput &input, const float dt) {
  if (!state.initialized) {
    state.initialized = true;
    state.pitch = input.fallback_pitch;
    state.camera_yaw = input.fallback_yaw;
    state.focus_target = input.focus_target;
  }

  state.active = input.active;
  if (input.active && input.has_pointer_delta) {
    const Vec3 rotation_accumulator{
        -input.pointer_delta.y * settings.pitch_sensitivity,
        input.pointer_delta.x * settings.yaw_sensitivity,
        0.0f,
    };
    state.camera_yaw = wrapRadians(state.camera_yaw + rotation_accumulator.y);
    state.pitch =
        std::clamp(state.pitch + rotation_accumulator.x, settings.min_pitch, settings.max_pitch);
  }

  const float snap_distance = std::max(settings.teleport_snap_distance, 0.0f);
  if (snap_distance > 0.0f && length(input.focus_target - state.focus_target) > snap_distance) {
    state.focus_target = input.focus_target;
  } else {
    state.focus_target =
        dampVec3(state.focus_target, input.focus_target, settings.target_response, dt);
  }

  return {.active = input.active,
          .camera_yaw = state.camera_yaw,
          .camera_pitch = state.pitch,
          .camera_target = state.focus_target};
}

Vec2 cameraRelativeMoveAxis(Vec2 local_axis, const float camera_yaw) {
  if (length(local_axis) > 1.0f) {
    local_axis = normalize(local_axis);
  }
  const Vec2 camera_forward{-std::sin(camera_yaw), -std::cos(camera_yaw)};
  const Vec2 camera_right{std::cos(camera_yaw), -std::sin(camera_yaw)};
  Vec2 world_axis = camera_right * local_axis.x + camera_forward * local_axis.y;
  if (length(world_axis) > 1.0f) {
    world_axis = normalize(world_axis);
  }
  return world_axis;
}

} // namespace aster
