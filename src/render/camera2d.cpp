// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/camera2d.hpp"

#include <algorithm>
#include <cmath>

namespace aster {
namespace {

[[nodiscard]] float clampf(const float value, const float min_value, const float max_value) {
  return std::min(std::max(value, min_value), max_value);
}

[[nodiscard]] Vec2 clampVec2(const Vec2 value, const Vec2 min_value, const Vec2 max_value) {
  return {clampf(value.x, min_value.x, max_value.x), clampf(value.y, min_value.y, max_value.y)};
}

} // namespace

void resetCamera2D(Camera2DState &state, const Vec2 target) {
  state.target = target;
  state.shake = {};
  state.shake_strength = 0.0f;
}

void addCameraShake2D(Camera2DState &state, const Vec2 impulse, const float strength) {
  state.shake = impulse;
  state.shake_strength = std::max(state.shake_strength, strength);
}

void updateCamera2D(Camera2DState &state, const Vec2 focus, const Camera2DConfig &config) {
  Vec2 desired = state.target;
  const Vec2 delta{focus.x - state.target.x, focus.y - state.target.y};
  if (std::abs(delta.x) > config.deadzone.x) {
    desired.x = focus.x - std::copysign(config.deadzone.x, delta.x);
  }
  if (std::abs(delta.y) > config.deadzone.y) {
    desired.y = focus.y - std::copysign(config.deadzone.y, delta.y);
  }
  desired = clampVec2(desired, config.min_target, config.max_target);
  const float smoothing = std::clamp(config.smoothing, 0.0f, 1.0f);
  state.target = {state.target.x + (desired.x - state.target.x) * smoothing,
                  state.target.y + (desired.y - state.target.y) * smoothing};
  state.shake_strength *= std::clamp(config.shake_decay, 0.0f, 1.0f);
  if (state.shake_strength < 0.0001f) {
    state.shake_strength = 0.0f;
    state.shake = {};
  }
}

OrbitCamera makeCamera2DOrbit(const Camera2DState &state, const Camera2DConfig &config) {
  OrbitCamera camera;
  const Vec2 offset{state.shake.x * state.shake_strength, state.shake.y * state.shake_strength};
  camera.target = {state.target.x + offset.x, 0.0f, state.target.y + offset.y};
  camera.yaw = 0.0f;
  camera.pitch = radians(88.0f);
  camera.radius = config.distance;
  camera.projection_mode = CameraProjectionMode::Orthographic;
  camera.orthographic_height = std::max(config.orthographic_height, 1.0f);
  camera.near_plane = 0.05f;
  camera.far_plane = 120.0f;
  return camera;
}

} // namespace aster
