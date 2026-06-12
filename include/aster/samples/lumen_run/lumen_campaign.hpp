// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/learning/learning_session.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

// LumenCampaign turns the Lumen Run sandbox into a staged expedition: every
// stage is grounded in an existing sandbox system (supply crate, cave
// traversal, mining, encounter combat, prism relay, construction yard, shard
// arena) and is observed purely through `LumenCampaignObservation` snapshots so
// the director stays deterministic and headless-testable.

enum class LumenCampaignStageId : std::uint32_t {
  Provision = 0,
  Descent,
  CoalVein,
  WebBreaker,
  SkitterHunt,
  PrismIgnition,
  SalvageContract,
  YardMastery,
};

inline constexpr std::size_t kLumenCampaignStageCount = 8u;

struct LumenCampaignStageSpec {
  LumenCampaignStageId id{};
  std::string_view key;
  std::string_view title;
  std::string_view briefing;
  int default_target = 1;
  int score_award = 100;
};

struct LumenCampaignObservation {
  float elapsed_seconds = 0.0f;
  int torches_held = 0;
  bool torch_equipped = false;
  float cave_interior = 0.0f;
  int mined_ores = 0;
  int cave_webs_cleared = 0;
  int cave_webs_total = 0;
  int skitters_defeated = 0;
  int skitters_total = 0;
  bool prism_relay_active = false;
  int processed_loads = 0;
  int delivered_bales = 0;
  bool construction_yard_complete = false;
  int lives = 3;
  int health = 20;
  bool defeated = false;
};

struct LumenCampaignStageProgress {
  LumenCampaignStageId id{};
  int progress = 0;
  int target = 1;
  bool started = false;
  bool completed = false;
  float started_seconds = -1.0f;
  float completed_seconds = -1.0f;
};

enum class LumenCampaignRank : std::uint32_t {
  S = 0,
  A,
  B,
  C,
  D,
};

[[nodiscard]] std::string_view lumenCampaignRankName(LumenCampaignRank rank);

struct LumenCampaignReport {
  bool completed = false;
  bool failed = false;
  int score = 0;
  int stage_score = 0;
  int time_bonus = 0;
  int death_penalty = 0;
  int deaths = 0;
  float elapsed_seconds = 0.0f;
  float completed_seconds = -1.0f;
  LumenCampaignRank rank = LumenCampaignRank::D;
  std::size_t active_stage_index = 0u;
  std::array<LumenCampaignStageProgress, kLumenCampaignStageCount> stages{};
};

class LumenCampaign {
public:
  LumenCampaign();

  void reset();
  void update(const LumenCampaignObservation &observation);

  [[nodiscard]] static const std::array<LumenCampaignStageSpec, kLumenCampaignStageCount> &
  stageSpecs();
  [[nodiscard]] static const LumenCampaignStageSpec &stageSpec(LumenCampaignStageId id);

  [[nodiscard]] const LumenCampaignStageProgress &stageProgress(LumenCampaignStageId id) const;
  [[nodiscard]] std::size_t activeStageIndex() const;
  [[nodiscard]] bool completed() const;
  [[nodiscard]] bool failed() const;
  [[nodiscard]] int score() const;
  [[nodiscard]] int deaths() const;
  [[nodiscard]] LumenCampaignRank rank() const;

  // One-line description of the active objective, e.g.
  // "Objective 3/8 - Coal Vein: mine coal ore (1/3)".
  [[nodiscard]] std::string objectiveLine() const;
  [[nodiscard]] std::vector<std::string> statusLines() const;
  [[nodiscard]] std::vector<LearningSignal> drainSignals();

  [[nodiscard]] LumenCampaignReport report() const;
  [[nodiscard]] std::string reportJson() const;
  [[nodiscard]] bool writeReportJson(const std::filesystem::path &path) const;

private:
  void emitSignal(std::string event, std::string_view stage_key, std::string stage_phase,
                  std::vector<std::string> channels,
                  std::map<std::string, std::string> metadata);
  [[nodiscard]] int stageScore() const;
  [[nodiscard]] int timeBonus() const;
  [[nodiscard]] int stageObservedProgress(const LumenCampaignStageSpec &spec,
                                          const LumenCampaignObservation &observation) const;
  [[nodiscard]] int stageObservedTarget(const LumenCampaignStageSpec &spec,
                                        const LumenCampaignObservation &observation) const;

  std::array<LumenCampaignStageProgress, kLumenCampaignStageCount> stages_{};
  std::vector<LearningSignal> pending_signals_;
  std::size_t active_stage_index_ = 0u;
  float elapsed_seconds_ = 0.0f;
  float completed_seconds_ = -1.0f;
  int deaths_ = 0;
  int last_lives_ = -1;
  bool failed_ = false;
  bool completed_ = false;
  bool completion_signal_emitted_ = false;
};

} // namespace aster
