// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/ui/classic_hud_signals.hpp"

#include <algorithm>

namespace aster {

ClassicHudSignalModel evaluateClassicHudSignals(const ClassicHudSignalInput input) {
  const float max_health = static_cast<float>(std::max(input.max_health, 1));
  ClassicHudSignalModel model;
  model.visible = input.gauntlet_active || input.threat_visible || input.mechanism_active ||
                  input.hurt_seconds > 0.0f;
  model.health_fraction =
      std::clamp(static_cast<float>(std::max(input.health, 0)) / max_health, 0.0f, 1.0f);
  model.hurt_flash = std::clamp(input.hurt_seconds * 2.2f, 0.0f, 1.0f);
  model.threat = input.threat_visible ? 1.0f : 0.0f;
  model.mechanism = input.mechanism_active ? 1.0f : 0.0f;
  return model;
}

} // namespace aster
