// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/samples/lumen_run/lumen_campaign.hpp"
#include "aster/samples/lumen_run/lumen_run.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

aster::LumenCampaignObservation baseObservation(const float seconds = 0.0f) {
  aster::LumenCampaignObservation observation;
  observation.elapsed_seconds = seconds;
  observation.cave_webs_total = 1;
  observation.skitters_total = 3;
  observation.lives = 3;
  observation.health = 20;
  return observation;
}

aster::LumenCampaignObservation fullProgressObservation(const float seconds) {
  aster::LumenCampaignObservation observation = baseObservation(seconds);
  observation.torches_held = 1;
  observation.torch_equipped = true;
  observation.cave_interior = 1.0f;
  observation.mined_ores = 3;
  observation.cave_webs_cleared = 1;
  observation.skitters_defeated = 3;
  observation.prism_relay_active = true;
  observation.processed_loads = 1;
  observation.delivered_bales = 1;
  observation.construction_yard_complete = true;
  return observation;
}

int countSignals(const std::vector<aster::LearningSignal> &signals, const char *event) {
  return static_cast<int>(std::count_if(
      signals.begin(), signals.end(),
      [event](const aster::LearningSignal &signal) { return signal.event == event; }));
}

void testCampaignStartsOnProvisionStage() {
  aster::LumenCampaign campaign;
  campaign.update(baseObservation());
  assert(campaign.activeStageIndex() == 0u);
  assert(!campaign.completed());
  assert(!campaign.failed());
  assert(campaign.score() == 0);
  const std::string objective = campaign.objectiveLine();
  assert(objective.find("Objective 1/8") != std::string::npos);
  assert(objective.find("Provision") != std::string::npos);
  const std::vector<aster::LearningSignal> signals = campaign.drainSignals();
  assert(countSignals(signals, "campaign_stage_started") == 1);
  assert(countSignals(signals, "campaign_stage_completed") == 0);
}

void testCampaignSequentialCompletionAndNoSkipping() {
  aster::LumenCampaign campaign;
  aster::LumenCampaignObservation observation = baseObservation();
  // Progress made out of order (skitters dead before torch pickup) accumulates
  // but never completes a later stage before the earlier ones.
  observation.skitters_defeated = 3;
  campaign.update(observation);
  assert(campaign.activeStageIndex() == 0u);
  assert(!campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).completed);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).progress == 3);

  observation.torches_held = 1;
  observation.elapsed_seconds = 10.0f;
  campaign.update(observation);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::Provision).completed);
  assert(campaign.activeStageIndex() == 1u);

  observation.cave_interior = 1.0f;
  observation.elapsed_seconds = 20.0f;
  campaign.update(observation);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::Descent).completed);
  assert(campaign.activeStageIndex() == 2u);

  observation.mined_ores = 3;
  observation.cave_webs_cleared = 1;
  observation.elapsed_seconds = 30.0f;
  campaign.update(observation);
  // CoalVein, WebBreaker, and the pre-banked SkitterHunt cascade together.
  assert(campaign.stageProgress(aster::LumenCampaignStageId::CoalVein).completed);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::WebBreaker).completed);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).completed);
  assert(campaign.activeStageIndex() == 5u);
  assert(!campaign.stageProgress(aster::LumenCampaignStageId::PrismIgnition).completed);
}

void testCampaignFullRunScoreAndRank() {
  aster::LumenCampaign campaign;
  campaign.update(baseObservation());
  campaign.update(fullProgressObservation(120.0f));
  assert(campaign.completed());
  assert(!campaign.failed());
  const aster::LumenCampaignReport report = campaign.report();
  assert(report.completed);
  assert(report.stage_score == 1000);
  assert(report.time_bonus > 400);
  assert(report.deaths == 0);
  assert(report.score == report.stage_score + report.time_bonus);
  assert(report.rank == aster::LumenCampaignRank::S);
  assert(campaign.objectiveLine().find("Expedition complete") != std::string::npos);
  for (const aster::LumenCampaignStageProgress &stage : report.stages) {
    assert(stage.completed);
    assert(stage.completed_seconds >= 0.0f);
  }
}

void testCampaignDeathPenaltyAndRankDrop() {
  aster::LumenCampaign campaign;
  aster::LumenCampaignObservation observation = baseObservation();
  campaign.update(observation);
  observation.lives = 1;
  observation.elapsed_seconds = 60.0f;
  campaign.update(observation);
  assert(campaign.deaths() == 2);

  campaign.update([] {
    aster::LumenCampaignObservation full = fullProgressObservation(400.0f);
    full.lives = 1;
    return full;
  }());
  assert(campaign.completed());
  const aster::LumenCampaignReport report = campaign.report();
  assert(report.deaths == 2);
  assert(report.death_penalty == 150);
  assert(report.score == report.stage_score + report.time_bonus - report.death_penalty);
  // Deaths disqualify rank S even with a high score.
  assert(report.rank != aster::LumenCampaignRank::S);
}

void testCampaignFailureAndReset() {
  aster::LumenCampaign campaign;
  aster::LumenCampaignObservation observation = baseObservation();
  observation.torches_held = 1;
  campaign.update(observation);
  (void)campaign.drainSignals();

  observation.defeated = true;
  observation.lives = 0;
  observation.elapsed_seconds = 90.0f;
  campaign.update(observation);
  assert(campaign.failed());
  assert(campaign.rank() == aster::LumenCampaignRank::D);
  assert(campaign.objectiveLine().find("Expedition failed") != std::string::npos);
  const std::vector<aster::LearningSignal> failure_signals = campaign.drainSignals();
  assert(countSignals(failure_signals, "campaign_failed") == 1);

  // Repeat observations do not double-emit the failure signal.
  campaign.update(observation);
  assert(countSignals(campaign.drainSignals(), "campaign_failed") == 0);

  campaign.reset();
  assert(!campaign.failed());
  assert(campaign.activeStageIndex() == 0u);
  assert(campaign.deaths() == 0);
  assert(campaign.score() == 0);
}

void testCampaignSignalsEmittedOncePerStage() {
  aster::LumenCampaign campaign;
  aster::LumenCampaignObservation observation = baseObservation();
  campaign.update(observation);
  observation.torches_held = 1;
  observation.elapsed_seconds = 5.0f;
  campaign.update(observation);
  campaign.update(observation);
  campaign.update(observation);
  const std::vector<aster::LearningSignal> signals = campaign.drainSignals();
  // Provision started + completed, Descent started; nothing repeats.
  assert(countSignals(signals, "campaign_stage_started") == 2);
  assert(countSignals(signals, "campaign_stage_completed") == 1);
  assert(campaign.drainSignals().empty());

  campaign.update(fullProgressObservation(200.0f));
  const std::vector<aster::LearningSignal> completion_signals = campaign.drainSignals();
  assert(countSignals(completion_signals, "campaign_stage_completed") == 7);
  assert(countSignals(completion_signals, "campaign_completed") == 1);
  campaign.update(fullProgressObservation(201.0f));
  assert(countSignals(campaign.drainSignals(), "campaign_completed") == 0);
}

void testCampaignDynamicTargetsFollowObservedTotals() {
  aster::LumenCampaign campaign;
  aster::LumenCampaignObservation observation = baseObservation();
  observation.cave_webs_total = 2;
  observation.skitters_total = 5;
  campaign.update(observation);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::WebBreaker).target == 2);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).target == 5);

  // Missing totals fall back to the authored defaults.
  aster::LumenCampaign fallback_campaign;
  aster::LumenCampaignObservation fallback = baseObservation();
  fallback.cave_webs_total = 0;
  fallback.skitters_total = 0;
  fallback_campaign.update(fallback);
  assert(fallback_campaign.stageProgress(aster::LumenCampaignStageId::WebBreaker).target ==
         aster::LumenCampaign::stageSpec(aster::LumenCampaignStageId::WebBreaker).default_target);
  assert(fallback_campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).target ==
         aster::LumenCampaign::stageSpec(aster::LumenCampaignStageId::SkitterHunt).default_target);
}

void testCampaignReportJsonRoundTrip() {
  aster::LumenCampaign campaign;
  campaign.update(baseObservation());
  campaign.update(fullProgressObservation(150.0f));
  const std::string json = campaign.reportJson();
  assert(json.find("\"campaign\": \"lumen_expedition\"") != std::string::npos);
  assert(json.find("\"completed\": true") != std::string::npos);
  assert(json.find("\"rank\": \"S\"") != std::string::npos);
  assert(json.find("\"id\": \"provision\"") != std::string::npos);
  assert(json.find("\"id\": \"yard_mastery\"") != std::string::npos);

  const std::filesystem::path report_path =
      std::filesystem::temp_directory_path() / "aster_lumen_campaign_test_report.json";
  std::filesystem::remove(report_path);
  assert(campaign.writeReportJson(report_path));
  std::ifstream stream(report_path);
  assert(stream.is_open());
  std::stringstream contents;
  contents << stream.rdbuf();
  assert(contents.str() == json);
  std::filesystem::remove(report_path);
}

void testCampaignObservationFromLiveSandbox() {
  // A real LumenRun spawn must produce a consistent campaign observation: all
  // encounter totals present, nothing completed, first stage active.
  aster::LumenRun game({.shard_count = 3, .sentinel_count = 0});
  const aster::LumenStatus &status = game.status();
  const aster::LumenSandboxStats sandbox = game.sandboxStats();
  assert(sandbox.cave_webs_total >= 1);
  assert(sandbox.skitters_total >= 1);
  assert(sandbox.cave_webs_cleared == 0);
  assert(sandbox.skitters_defeated == 0);

  aster::LumenCampaignObservation observation;
  observation.elapsed_seconds = status.elapsed_seconds;
  observation.torches_held = game.torchCount();
  observation.torch_equipped = game.equippedLight().has_value();
  observation.cave_interior = game.caveLightingState().interior;
  observation.mined_ores = sandbox.mined_ores;
  observation.cave_webs_cleared = sandbox.cave_webs_cleared;
  observation.cave_webs_total = sandbox.cave_webs_total;
  observation.skitters_defeated = sandbox.skitters_defeated;
  observation.skitters_total = sandbox.skitters_total;
  observation.prism_relay_active = sandbox.prism_relay_active;
  observation.processed_loads = sandbox.processed_loads;
  observation.delivered_bales = sandbox.delivered_bales;
  observation.construction_yard_complete = sandbox.construction_yard_complete;
  observation.lives = status.lives;
  observation.health = status.health;
  observation.defeated = status.defeated;

  aster::LumenCampaign campaign;
  campaign.update(observation);
  assert(campaign.activeStageIndex() == 0u);
  assert(!campaign.completed());
  assert(!campaign.failed());
  assert(campaign.stageProgress(aster::LumenCampaignStageId::WebBreaker).target ==
         sandbox.cave_webs_total);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::SkitterHunt).target ==
         sandbox.skitters_total);
  assert(campaign.stageProgress(aster::LumenCampaignStageId::YardMastery).target == 1);
  assert(!campaign.stageProgress(aster::LumenCampaignStageId::YardMastery).completed);
}

} // namespace

struct NamedCampaignTest {
  const char *name = "";
  void (*run)() = nullptr;
};

static bool campaignTestSelected(const char *name, const int argc, const char **argv) {
  if (argc <= 1) {
    return true;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], name) == 0) {
      return true;
    }
  }
  return false;
}

int main(const int argc, const char **argv) {
  const NamedCampaignTest tests[] = {
      {"campaign_starts_on_provision_stage", testCampaignStartsOnProvisionStage},
      {"campaign_sequential_completion_and_no_skipping",
       testCampaignSequentialCompletionAndNoSkipping},
      {"campaign_full_run_score_and_rank", testCampaignFullRunScoreAndRank},
      {"campaign_death_penalty_and_rank_drop", testCampaignDeathPenaltyAndRankDrop},
      {"campaign_failure_and_reset", testCampaignFailureAndReset},
      {"campaign_signals_emitted_once_per_stage", testCampaignSignalsEmittedOncePerStage},
      {"campaign_dynamic_targets_follow_observed_totals",
       testCampaignDynamicTargetsFollowObservedTotals},
      {"campaign_report_json_round_trip", testCampaignReportJsonRoundTrip},
      {"campaign_observation_from_live_sandbox", testCampaignObservationFromLiveSandbox},
  };
  bool ran = false;
  for (const NamedCampaignTest &test : tests) {
    if (!campaignTestSelected(test.name, argc, argv)) {
      continue;
    }
    std::cout << "running " << test.name << '\n';
    test.run();
    ran = true;
  }
  assert(ran);
  std::cout << "lumen_campaign_tests passed.\n";
  return 0;
}
