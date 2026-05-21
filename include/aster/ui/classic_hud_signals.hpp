// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

namespace aster {

struct ClassicHudSignalInput {
  int health = 0;
  int max_health = 1;
  bool gauntlet_active = false;
  bool threat_visible = false;
  bool mechanism_active = false;
  float hurt_seconds = 0.0f;
};

struct ClassicHudSignalModel {
  bool visible = false;
  float health_fraction = 1.0f;
  float hurt_flash = 0.0f;
  float threat = 0.0f;
  float mechanism = 0.0f;
};

[[nodiscard]] ClassicHudSignalModel evaluateClassicHudSignals(ClassicHudSignalInput input);

} // namespace aster
