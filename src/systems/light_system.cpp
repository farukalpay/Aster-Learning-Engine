// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/systems/light_system.hpp"

#include <algorithm>
#include <cmath>

namespace aster {

DynamicPointLight evaluateFlickerLight(const FlickerLightSpec &spec, const Vec3 position,
                                       const float seconds) {
  const float primary = std::sin(seconds * spec.speed);
  const float secondary = std::sin(seconds * (spec.speed * 1.71f) + 1.9f);
  const float flicker = 1.0f + spec.amplitude * (primary * 0.64f + secondary * 0.36f);
  return {true, position, spec.color, spec.intensity * std::max(0.20f, flicker),
          std::max(spec.source_radius, 0.0f)};
}

} // namespace aster
