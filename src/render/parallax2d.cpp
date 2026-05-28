// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/render/parallax2d.hpp"

#include <cmath>

namespace aster {

Vec2 parallaxPosition2D(const ParallaxLayer2D &layer, const Vec2 camera_target) {
  Vec2 position{layer.base_position.x + camera_target.x * layer.scroll_scale.x,
                layer.base_position.y + camera_target.y * layer.scroll_scale.y};
  if (layer.repeat_size.x > 0.001f) {
    position.x -= std::floor(position.x / layer.repeat_size.x) * layer.repeat_size.x;
  }
  if (layer.repeat_size.y > 0.001f) {
    position.y -= std::floor(position.y / layer.repeat_size.y) * layer.repeat_size.y;
  }
  return position;
}

} // namespace aster
