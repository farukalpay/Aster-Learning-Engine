// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/deterministic_sim.hpp"
#include "aster/math/vec.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace aster {

enum class TuzoraOutcome : std::uint32_t {
  Playing,
  SignalLit,
  Nightfall,
};

enum class TuzoraBuildSlot : std::uint32_t {
  SandStone,
  WoodPlank,
  ShadeCloth,
  Lamp,
  SignalPart,
};

struct TuzoraTuning {
  std::uint32_t seed = 0x7A2026u;
  int day_ticks = 60 * 110;
  bool debug_tiles = false;
};

struct TuzoraStatus {
  std::uint32_t tick = 0u;
  Vec2 player{};
  Vec2 velocity{};
  bool on_ground = false;
  int facing = 1;
  TuzoraOutcome outcome = TuzoraOutcome::Playing;
  TuzoraBuildSlot selected_slot = TuzoraBuildSlot::WoodPlank;
  int shells = 0;
  int wood = 0;
  int scrap = 0;
  int shade_cloth = 0;
  int signal_parts = 0;
  int signal_parts_installed = 0;
  int lamps_placed = 0;
  int blocks_placed = 0;
  int blocks_broken = 0;
  int pickups_collected = 0;
  float day_fraction = 0.0f;
  bool signal_lit = false;
  bool night_started = false;
  std::uint32_t replay_checksum = 0u;
  std::uint64_t world_hash = 0u;
  std::string prompt_line;
  std::string event_line;
};

struct TuzoraHudModel {
  std::string title;
  std::string objective;
  std::string status_line;
  std::string resource_line;
  std::string hotbar_line;
  std::string prompt_line;
  std::string event_line;
  float day_fraction = 0.0f;
  bool ended = false;
  bool signal_lit = false;
};

class Tuzora {
public:
  explicit Tuzora(TuzoraTuning tuning = {});
  ~Tuzora();

  Tuzora(const Tuzora &) = delete;
  Tuzora &operator=(const Tuzora &) = delete;
  Tuzora(Tuzora &&) noexcept;
  Tuzora &operator=(Tuzora &&) noexcept;

  void reset();
  void updateFixed(SimCommand command);

  [[nodiscard]] SimCommand scriptedCommand(std::uint32_t tick) const;
  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const TuzoraStatus &status() const;
  [[nodiscard]] const CommandReplay &replay() const;
  [[nodiscard]] TuzoraHudModel hudModel() const;
  [[nodiscard]] Vec2 cameraTarget() const;
  [[nodiscard]] std::uint64_t worldHash() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace aster
