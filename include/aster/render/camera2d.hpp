// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"
#include "aster/render/camera.hpp"

namespace aster {

struct Camera2DConfig {
  Vec2 min_target{-1000.0f, -1000.0f};
  Vec2 max_target{1000.0f, 1000.0f};
  Vec2 deadzone{0.85f, 0.48f};
  float smoothing = 0.18f;
  float orthographic_height = 12.0f;
  float distance = 26.0f;
  float shake_decay = 0.82f;
};

struct Camera2DState {
  Vec2 target{};
  Vec2 shake{};
  float shake_strength = 0.0f;
};

void resetCamera2D(Camera2DState &state, Vec2 target);
void addCameraShake2D(Camera2DState &state, Vec2 impulse, float strength);
void updateCamera2D(Camera2DState &state, Vec2 focus, const Camera2DConfig &config);
[[nodiscard]] OrbitCamera makeCamera2DOrbit(const Camera2DState &state,
                                            const Camera2DConfig &config);

} // namespace aster
