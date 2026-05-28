// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/math/vec.hpp"

#include <string>

namespace aster {

struct ParallaxLayer2D {
  std::string name;
  Vec2 base_position{};
  Vec2 scroll_scale{0.0f, 0.0f};
  Vec2 repeat_size{};
  float depth = -8.0f;
};

[[nodiscard]] Vec2 parallaxPosition2D(const ParallaxLayer2D &layer, Vec2 camera_target);

} // namespace aster
