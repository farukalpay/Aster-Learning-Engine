// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/deterministic_sim.hpp"
#include "aster/math/vec.hpp"
#include "aster/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aster {

enum class SparkThresholdOutcome : std::uint32_t {
  Playing,
  ResonanceEnding,
  EscapeEnding,
  OverloadEnding,
  Defeated,
};

enum class SparkThresholdScriptRoute : std::uint32_t {
  Resonance,
  Escape,
  Overload,
};

enum class SparkThresholdNode : std::uint32_t {
  Mine,
  Scrap,
  Castle,
};

struct SparkThresholdTuning {
  std::uint32_t seed = 0xA57E5A7Eu;
  int time_limit_ticks = 60 * 60 * 7;
  float walk_speed = 2.85f;
  float run_speed = 4.85f;
  float interact_radius = 1.45f;
  float gate_radius = 1.70f;
  bool title_screen = true;
};

struct SparkThresholdGuideStep {
  std::string title;
  std::string body;
  std::string hint;
  bool completed = false;
};

struct SparkThresholdStatus {
  std::uint32_t tick = 0;
  float elapsed_seconds = 0.0f;
  float time_remaining_seconds = 0.0f;
  SparkThresholdOutcome outcome = SparkThresholdOutcome::Playing;
  bool title_screen = true;
  int health = 3;
  int total_nodes = 3;
  int activated_nodes = 0;
  int overload = 0;
  bool mine_active = false;
  bool scrap_active = false;
  bool castle_active = false;
  bool final_gate_live = false;
  SparkThresholdNode first_node = SparkThresholdNode::Mine;
  bool has_first_node = false;
  std::uint32_t replay_checksum = 0u;
  std::uint64_t world_hash = 0u;
  std::string ending_key;
};

struct SparkThresholdHudModel {
  std::string title;
  std::string subtitle;
  std::string objective;
  std::string status_line;
  std::string route_hint;
  std::string prompt_line;
  std::string guide_title;
  std::string guide_body;
  std::string guide_hint;
  std::string ending_title;
  std::string ending_body;
  float time_fraction = 1.0f;
  float node_fraction = 0.0f;
  bool title_screen = true;
  bool playing = false;
  bool ended = false;
  SparkThresholdOutcome outcome = SparkThresholdOutcome::Playing;
};

class SparkThreshold {
public:
  explicit SparkThreshold(SparkThresholdTuning tuning = {});
  ~SparkThreshold();

  SparkThreshold(const SparkThreshold &) = delete;
  SparkThreshold &operator=(const SparkThreshold &) = delete;
  SparkThreshold(SparkThreshold &&) noexcept;
  SparkThreshold &operator=(SparkThreshold &&) noexcept;

  void reset();
  void startRun();
  void updateFixed(SimCommand command);

  [[nodiscard]] SimCommand scriptedCommand(SparkThresholdScriptRoute route) const;
  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const SparkThresholdStatus &status() const;
  [[nodiscard]] const CommandReplay &replay() const;
  [[nodiscard]] SparkThresholdHudModel hudModel() const;
  [[nodiscard]] SparkThresholdGuideStep guideStep() const;
  [[nodiscard]] Vec3 playerPosition() const;
  [[nodiscard]] Vec3 cameraTarget() const;
  [[nodiscard]] std::uint64_t worldHash() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace aster
