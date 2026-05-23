// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/deterministic_sim.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace aster {

struct AsterGridCoord {
  int x = 0;
  int y = 0;
};

enum class AsterGridTacticsOutcome : std::uint32_t {
  Playing,
  Victory,
  Defeated,
};

enum class AsterGridTacticsVisualEvent : std::uint32_t {
  None,
  TerminalHack,
  EnergyReroute,
  TurretOffline,
  ExtractionOpen,
  ExtractionComplete,
  Defeat,
};

struct AsterGridTacticsTuning {
  std::uint32_t seed = 0xA57E2026u;
  int width = 18;
  int height = 12;
  int overload_ticks = 60 * 90;
  int player_step_ticks = 7;
  int guard_step_ticks = 18;
  int turret_fire_ticks = 36;
  bool debug_grid = false;
};

struct AsterGridTacticsStatus {
  std::uint32_t tick = 0;
  AsterGridCoord player{};
  AsterGridTacticsOutcome outcome = AsterGridTacticsOutcome::Playing;
  int overload_ticks_remaining = 0;
  int energy_nodes_rerouted = 0;
  int total_energy_nodes = 0;
  int terminals_hacked = 0;
  bool door_open = false;
  bool turret_disabled = false;
  AsterGridTacticsVisualEvent visual_event = AsterGridTacticsVisualEvent::None;
  std::uint32_t visual_event_tick = 0u;
  std::uint32_t replay_checksum = 0u;
  std::uint64_t world_hash = 0u;
  std::string defeat_reason;
};

struct AsterGridTacticsHudModel {
  std::string title;
  std::string objective;
  std::string status_line;
  std::string power_label;
  std::string terminal_label;
  std::string exit_label;
  std::string callout_line;
  float overload_fraction = 1.0f;
  bool terminal_hacked = false;
  bool door_open = false;
  bool turret_disabled = false;
  bool victory = false;
  bool defeated = false;
};

class AsterGridTactics {
public:
  explicit AsterGridTactics(AsterGridTacticsTuning tuning = {});
  ~AsterGridTactics();

  AsterGridTactics(const AsterGridTactics &) = delete;
  AsterGridTactics &operator=(const AsterGridTactics &) = delete;
  AsterGridTactics(AsterGridTactics &&) noexcept;
  AsterGridTactics &operator=(AsterGridTactics &&) noexcept;

  void reset();
  void updateFixed(SimCommand command);

  [[nodiscard]] SimCommand scriptedCommand(std::uint32_t tick) const;
  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const AsterGridTacticsStatus &status() const;
  [[nodiscard]] const CommandReplay &replay() const;
  [[nodiscard]] AsterGridTacticsHudModel hudModel() const;
  [[nodiscard]] std::uint64_t worldHash() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace aster
