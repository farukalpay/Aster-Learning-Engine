// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/math/vec.hpp"

namespace aster {

struct DynamicPointLight {
  bool active = false;
  Vec3 position{};
  Vec3 color{1.0f, 0.70f, 0.36f};
  float intensity = 1.0f;
  float source_radius = 0.0f;
};

struct FlickerLightSpec {
  Vec3 color{1.0f, 0.56f, 0.24f};
  float intensity = 14.0f;
  float amplitude = 0.28f;
  float speed = 12.0f;
  float source_radius = 0.0f;
};

[[nodiscard]] DynamicPointLight evaluateFlickerLight(const FlickerLightSpec &spec, Vec3 position,
                                                     float seconds);

} // namespace aster
