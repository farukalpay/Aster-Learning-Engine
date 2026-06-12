// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/lumen_run/lumen_campaign.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace aster {

namespace {

constexpr float kCaveInteriorEntryThreshold = 0.6f;
constexpr int kDeathPenaltyPerLife = 75;
constexpr int kMaxTimeBonus = 500;
constexpr float kTimeBonusWindowSeconds = 900.0f;

constexpr std::array<LumenCampaignStageSpec, kLumenCampaignStageCount> kStageSpecs{{
    {LumenCampaignStageId::Provision, "provision", "Provision",
     "take a torch from the supply crate", 1, 50},
    {LumenCampaignStageId::Descent, "descent", "Descent", "carry the light into the cave", 1, 75},
    {LumenCampaignStageId::CoalVein, "coal_vein", "Coal Vein", "mine the coal ore nodes", 3, 150},
    {LumenCampaignStageId::WebBreaker, "web_breaker", "Web Breaker",
     "cut the cave webs blocking the tunnel", 1, 100},
    {LumenCampaignStageId::SkitterHunt, "skitter_hunt", "Skitter Hunt",
     "defeat the cave skitters", 3, 150},
    {LumenCampaignStageId::PrismIgnition, "prism_ignition", "Prism Ignition",
     "ignite the prism relay", 1, 125},
    {LumenCampaignStageId::SalvageContract, "salvage_contract", "Salvage Contract",
     "process a shredder load and deliver a bale", 2, 200},
    {LumenCampaignStageId::YardMastery, "yard_mastery", "Yard Mastery",
     "complete the salvage yard contract", 1, 150},
}};

std::string jsonEscape(const std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (const char value : text) {
    switch (value) {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += value;
      break;
    }
  }
  return escaped;
}

std::string formatSeconds(const float seconds) {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(2);
  out << seconds;
  return out.str();
}

} // namespace

std::string_view lumenCampaignRankName(const LumenCampaignRank rank) {
  switch (rank) {
  case LumenCampaignRank::S:
    return "S";
  case LumenCampaignRank::A:
    return "A";
  case LumenCampaignRank::B:
    return "B";
  case LumenCampaignRank::C:
    return "C";
  case LumenCampaignRank::D:
    return "D";
  }
  return "D";
}

LumenCampaign::LumenCampaign() {
  reset();
}

void LumenCampaign::reset() {
  for (std::size_t index = 0; index < kLumenCampaignStageCount; ++index) {
    stages_[index] = LumenCampaignStageProgress{};
    stages_[index].id = kStageSpecs[index].id;
    stages_[index].target = kStageSpecs[index].default_target;
  }
  pending_signals_.clear();
  active_stage_index_ = 0u;
  elapsed_seconds_ = 0.0f;
  completed_seconds_ = -1.0f;
  deaths_ = 0;
  last_lives_ = -1;
  failed_ = false;
  completed_ = false;
  completion_signal_emitted_ = false;
}

const std::array<LumenCampaignStageSpec, kLumenCampaignStageCount> &LumenCampaign::stageSpecs() {
  return kStageSpecs;
}

const LumenCampaignStageSpec &LumenCampaign::stageSpec(const LumenCampaignStageId id) {
  return kStageSpecs[static_cast<std::size_t>(id)];
}

const LumenCampaignStageProgress &
LumenCampaign::stageProgress(const LumenCampaignStageId id) const {
  return stages_[static_cast<std::size_t>(id)];
}

std::size_t LumenCampaign::activeStageIndex() const {
  return active_stage_index_;
}

bool LumenCampaign::completed() const {
  return completed_;
}

bool LumenCampaign::failed() const {
  return failed_;
}

int LumenCampaign::deaths() const {
  return deaths_;
}

int LumenCampaign::stageObservedProgress(const LumenCampaignStageSpec &spec,
                                         const LumenCampaignObservation &observation) const {
  switch (spec.id) {
  case LumenCampaignStageId::Provision:
    return (observation.torches_held > 0 || observation.torch_equipped) ? 1 : 0;
  case LumenCampaignStageId::Descent:
    return observation.cave_interior >= kCaveInteriorEntryThreshold ? 1 : 0;
  case LumenCampaignStageId::CoalVein:
    return observation.mined_ores;
  case LumenCampaignStageId::WebBreaker:
    return observation.cave_webs_cleared;
  case LumenCampaignStageId::SkitterHunt:
    return observation.skitters_defeated;
  case LumenCampaignStageId::PrismIgnition:
    return observation.prism_relay_active ? 1 : 0;
  case LumenCampaignStageId::SalvageContract:
    return std::min(observation.processed_loads, 1) + std::min(observation.delivered_bales, 1);
  case LumenCampaignStageId::YardMastery:
    return observation.construction_yard_complete ? 1 : 0;
  }
  return 0;
}

int LumenCampaign::stageObservedTarget(const LumenCampaignStageSpec &spec,
                                       const LumenCampaignObservation &observation) const {
  switch (spec.id) {
  case LumenCampaignStageId::WebBreaker:
    return observation.cave_webs_total > 0 ? observation.cave_webs_total : spec.default_target;
  case LumenCampaignStageId::SkitterHunt:
    return observation.skitters_total > 0 ? observation.skitters_total : spec.default_target;
  default:
    return spec.default_target;
  }
}

void LumenCampaign::emitSignal(std::string event, const std::string_view stage_key,
                               std::string stage_phase, std::vector<std::string> channels,
                               std::map<std::string, std::string> metadata) {
  LearningSignal signal;
  signal.event = std::move(event);
  signal.asset = "campaign.lumen_expedition";
  signal.channels = std::move(channels);
  signal.channels.push_back("campaign." + std::string(stage_key));
  signal.metadata = std::move(metadata);
  signal.stage = std::move(stage_phase);
  pending_signals_.push_back(std::move(signal));
}

void LumenCampaign::update(const LumenCampaignObservation &observation) {
  elapsed_seconds_ = std::max(elapsed_seconds_, observation.elapsed_seconds);

  if (last_lives_ < 0) {
    last_lives_ = observation.lives;
  } else if (observation.lives < last_lives_) {
    deaths_ += last_lives_ - observation.lives;
    last_lives_ = observation.lives;
  } else if (observation.lives > last_lives_) {
    last_lives_ = observation.lives;
  }

  if (observation.defeated && !failed_) {
    failed_ = true;
    emitSignal("campaign_failed", "expedition", "evaluate", {"campaign.outcome"},
               {{"seconds", formatSeconds(elapsed_seconds_)},
                {"deaths", std::to_string(deaths_)}});
  }

  // Progress accumulates from every observation so out-of-order play still
  // counts; completion stays sequential so the expedition reads as a campaign.
  for (std::size_t index = 0; index < kLumenCampaignStageCount; ++index) {
    const LumenCampaignStageSpec &spec = kStageSpecs[index];
    LumenCampaignStageProgress &stage = stages_[index];
    stage.target = std::max(1, stageObservedTarget(spec, observation));
    stage.progress =
        std::clamp(std::max(stage.progress, stageObservedProgress(spec, observation)), 0,
                   stage.target);
  }

  if (active_stage_index_ < kLumenCampaignStageCount &&
      !stages_[active_stage_index_].started) {
    LumenCampaignStageProgress &stage = stages_[active_stage_index_];
    stage.started = true;
    stage.started_seconds = observation.elapsed_seconds;
    const LumenCampaignStageSpec &spec = kStageSpecs[active_stage_index_];
    emitSignal("campaign_stage_started", spec.key, "teach", {"campaign.stage"},
               {{"stage", std::string(spec.key)},
                {"index", std::to_string(active_stage_index_ + 1)},
                {"target", std::to_string(stage.target)}});
  }

  while (active_stage_index_ < kLumenCampaignStageCount) {
    LumenCampaignStageProgress &stage = stages_[active_stage_index_];
    if (stage.progress < stage.target) {
      break;
    }
    if (!stage.started) {
      stage.started = true;
      stage.started_seconds = observation.elapsed_seconds;
    }
    stage.completed = true;
    stage.completed_seconds = observation.elapsed_seconds;
    const LumenCampaignStageSpec &spec = kStageSpecs[active_stage_index_];
    emitSignal("campaign_stage_completed", spec.key, "evaluate", {"campaign.stage"},
               {{"stage", std::string(spec.key)},
                {"progress", std::to_string(stage.progress)},
                {"target", std::to_string(stage.target)},
                {"seconds", formatSeconds(observation.elapsed_seconds)}});
    ++active_stage_index_;
    if (active_stage_index_ < kLumenCampaignStageCount) {
      LumenCampaignStageProgress &next_stage = stages_[active_stage_index_];
      if (!next_stage.started) {
        next_stage.started = true;
        next_stage.started_seconds = observation.elapsed_seconds;
        const LumenCampaignStageSpec &next_spec = kStageSpecs[active_stage_index_];
        emitSignal("campaign_stage_started", next_spec.key, "teach", {"campaign.stage"},
                   {{"stage", std::string(next_spec.key)},
                    {"index", std::to_string(active_stage_index_ + 1)},
                    {"target", std::to_string(next_stage.target)}});
      }
    }
  }

  if (!completed_ && active_stage_index_ >= kLumenCampaignStageCount) {
    completed_ = true;
    completed_seconds_ = observation.elapsed_seconds;
  }

  if (completed_ && !completion_signal_emitted_) {
    completion_signal_emitted_ = true;
    emitSignal("campaign_completed", "expedition", "evaluate", {"campaign.outcome"},
               {{"score", std::to_string(score())},
                {"rank", std::string(lumenCampaignRankName(rank()))},
                {"deaths", std::to_string(deaths_)},
                {"seconds", formatSeconds(completed_seconds_)}});
  }
}

int LumenCampaign::stageScore() const {
  int stage_score = 0;
  for (std::size_t index = 0; index < kLumenCampaignStageCount; ++index) {
    if (stages_[index].completed) {
      stage_score += kStageSpecs[index].score_award;
    }
  }
  return stage_score;
}

int LumenCampaign::timeBonus() const {
  if (!completed_ || completed_seconds_ < 0.0f) {
    return 0;
  }
  const float remaining = std::max(0.0f, 1.0f - completed_seconds_ / kTimeBonusWindowSeconds);
  return static_cast<int>(std::lround(remaining * static_cast<float>(kMaxTimeBonus)));
}

int LumenCampaign::score() const {
  return std::max(0, stageScore() + timeBonus() - deaths_ * kDeathPenaltyPerLife);
}

LumenCampaignRank LumenCampaign::rank() const {
  if (failed_ || !completed_) {
    return LumenCampaignRank::D;
  }
  const int value = score();
  if (value >= 1300 && deaths_ == 0) {
    return LumenCampaignRank::S;
  }
  if (value >= 1050) {
    return LumenCampaignRank::A;
  }
  if (value >= 850) {
    return LumenCampaignRank::B;
  }
  return LumenCampaignRank::C;
}

std::string LumenCampaign::objectiveLine() const {
  if (failed_) {
    return "Expedition failed - press R to mount a new attempt.";
  }
  if (completed_) {
    std::string line = "Expedition complete - rank ";
    line += lumenCampaignRankName(rank());
    line += ", score " + std::to_string(score());
    return line;
  }
  const std::size_t index = std::min(active_stage_index_, kLumenCampaignStageCount - 1u);
  const LumenCampaignStageSpec &spec = kStageSpecs[index];
  const LumenCampaignStageProgress &stage = stages_[index];
  std::string line = "Objective " + std::to_string(index + 1) + "/" +
                     std::to_string(kLumenCampaignStageCount) + " - ";
  line += spec.title;
  line += ": ";
  line += spec.briefing;
  line += " (" + std::to_string(stage.progress) + "/" + std::to_string(stage.target) + ")";
  return line;
}

std::vector<std::string> LumenCampaign::statusLines() const {
  std::vector<std::string> lines;
  lines.push_back("campaign: " + objectiveLine());
  std::string tally = "campaign score: " + std::to_string(score()) + ", deaths " +
                      std::to_string(deaths_);
  if (completed_) {
    tally += ", rank ";
    tally += lumenCampaignRankName(rank());
  }
  lines.push_back(std::move(tally));
  return lines;
}

std::vector<LearningSignal> LumenCampaign::drainSignals() {
  std::vector<LearningSignal> signals;
  signals.swap(pending_signals_);
  return signals;
}

LumenCampaignReport LumenCampaign::report() const {
  LumenCampaignReport result;
  result.completed = completed_;
  result.failed = failed_;
  result.deaths = deaths_;
  result.death_penalty = deaths_ * kDeathPenaltyPerLife;
  result.elapsed_seconds = elapsed_seconds_;
  result.completed_seconds = completed_seconds_;
  result.active_stage_index = active_stage_index_;
  result.stages = stages_;
  result.stage_score = stageScore();
  result.time_bonus = timeBonus();
  result.score = score();
  result.rank = rank();
  return result;
}

std::string LumenCampaign::reportJson() const {
  const LumenCampaignReport summary = report();
  std::ostringstream out;
  out << "{\n";
  out << "  \"campaign\": \"lumen_expedition\",\n";
  out << "  \"completed\": " << (summary.completed ? "true" : "false") << ",\n";
  out << "  \"failed\": " << (summary.failed ? "true" : "false") << ",\n";
  out << "  \"score\": " << summary.score << ",\n";
  out << "  \"stage_score\": " << summary.stage_score << ",\n";
  out << "  \"time_bonus\": " << summary.time_bonus << ",\n";
  out << "  \"death_penalty\": " << summary.death_penalty << ",\n";
  out << "  \"deaths\": " << summary.deaths << ",\n";
  out << "  \"rank\": \"" << lumenCampaignRankName(summary.rank) << "\",\n";
  out << "  \"elapsed_seconds\": " << formatSeconds(summary.elapsed_seconds) << ",\n";
  out << "  \"completed_seconds\": " << formatSeconds(summary.completed_seconds) << ",\n";
  out << "  \"active_stage_index\": " << summary.active_stage_index << ",\n";
  out << "  \"stages\": [\n";
  for (std::size_t index = 0; index < kLumenCampaignStageCount; ++index) {
    const LumenCampaignStageSpec &spec = kStageSpecs[index];
    const LumenCampaignStageProgress &stage = summary.stages[index];
    out << "    {\"id\": \"" << jsonEscape(spec.key) << "\", \"title\": \""
        << jsonEscape(spec.title) << "\", \"progress\": " << stage.progress
        << ", \"target\": " << stage.target
        << ", \"completed\": " << (stage.completed ? "true" : "false")
        << ", \"started_seconds\": " << formatSeconds(stage.started_seconds)
        << ", \"completed_seconds\": " << formatSeconds(stage.completed_seconds)
        << ", \"score_award\": " << spec.score_award << "}"
        << (index + 1 < kLumenCampaignStageCount ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

bool LumenCampaign::writeReportJson(const std::filesystem::path &path) const {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
  }
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << reportJson();
  return out.good();
}

} // namespace aster
