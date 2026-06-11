// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/learning/learning_runtime.hpp"
#include "aster/learning/learning_session.hpp"
#include "aster/learning/memory_controller.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

aster::LearningRuntimeContract lumenContract() {
  return {.lesson_id = "lesson.lumen_mining",
          .workflow_stages = {"diagnose", "design", "teach", "evaluate"},
          .safety_evidence_ids = {"evidence.pickaxe_pickup", "evidence.torch_use",
                                  "evidence.ore_identified", "evidence.mine_attempt",
                                  "evidence.feedback_read"},
          .objectives = {{"objective.pickup_tool", {"evidence.pickaxe_pickup"}},
                         {"objective.use_torch", {"evidence.torch_use"}},
                         {"objective.identify_ore", {"evidence.ore_identified"}},
                         {"objective.mine_ore", {"evidence.mine_attempt"}},
                         {"objective.read_feedback", {"evidence.feedback_read"}}},
          .scaffold_rules = {{"scaffold.pickaxe_prompt",
                              "design",
                              {"evidence.mine_attempt"},
                              {"hypothesis.tool_affordance_gap"},
                              false},
                             {"scaffold.torch_prompt",
                              "design",
                              {"evidence.torch_use"},
                              {"hypothesis.torch_model_gap"},
                              false}}};
}

aster::LearningTraceForest goodForest() {
  aster::LearningTraceForest forest;
  forest.addEvent({.id = "t1",
                   .stage = "diagnose",
                   .evidence_id = "evidence.mine_attempt",
                   .hypothesis_id = "hypothesis.tool_affordance_gap"});
  forest.addEvent(
      {.id = "t2",
       .stage = "design",
       .scaffold_id = "scaffold.pickaxe_prompt",
       .metadata = {
           {"rationale", "evidence.mine_attempt supports hypothesis.tool_affordance_gap"}}});
  forest.addEvent({.id = "t3", .stage = "teach", .evidence_id = "evidence.pickaxe_pickup"});
  forest.addEvent({.id = "t4", .stage = "teach", .evidence_id = "evidence.torch_use"});
  forest.addEvent({.id = "t5", .stage = "teach", .evidence_id = "evidence.ore_identified"});
  forest.addEvent({.id = "t6", .stage = "teach", .evidence_id = "evidence.feedback_read"});
  forest.addEvent({.id = "t7",
                   .stage = "evaluate",
                   .evidence_id = "evidence.feedback_read",
                   .claims_mastery = true});
  return forest;
}

void testGoodLumenTrace() {
  const aster::LearningRuntimeReport report =
      aster::evaluateLearningRuntime(lumenContract(), goodForest());
  assert(report.passed);
  assert(report.objective_coverage == 1.0f);
  assert(report.workflow_coverage == 1.0f);
  assert(report.decisions.size() == 1u);
  assert(report.decisions.front().accepted);
}

void testMissingToolEvidence() {
  aster::LearningTraceForest forest = goodForest();
  aster::LearningRuntimeContract contract = lumenContract();
  contract.objectives[0].evidence_ids = {"evidence.pickaxe_pickup_missing"};
  const aster::LearningRuntimeReport report = aster::evaluateLearningRuntime(contract, forest);
  assert(!report.passed);
  assert(report.objective_coverage < 1.0f);
}

void testUnsupportedScaffoldRationale() {
  aster::LearningTraceForest forest;
  forest.addEvent({.stage = "diagnose",
                   .evidence_id = "evidence.mine_attempt",
                   .hypothesis_id = "hypothesis.tool_affordance_gap"});
  forest.addEvent({.stage = "design",
                   .scaffold_id = "scaffold.pickaxe_prompt",
                   .metadata = {{"rationale", "generic encouragement"}}});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.pickaxe_pickup"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.torch_use"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.ore_identified"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.feedback_read"});
  forest.addEvent({.stage = "evaluate", .evidence_id = "evidence.feedback_read"});
  const aster::LearningRuntimeReport report =
      aster::evaluateLearningRuntime(lumenContract(), forest);
  assert(!report.passed);
  assert(!report.decisions.empty());
  assert(!report.decisions.front().accepted);
}

void testFalseMasteryRejected() {
  aster::LearningTraceForest forest;
  forest.addEvent({.stage = "diagnose",
                   .evidence_id = "evidence.mine_attempt",
                   .hypothesis_id = "hypothesis.tool_affordance_gap",
                   .claims_mastery = true});
  forest.addEvent(
      {.stage = "design",
       .scaffold_id = "scaffold.pickaxe_prompt",
       .metadata = {
           {"rationale", "evidence.mine_attempt supports hypothesis.tool_affordance_gap"}}});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.pickaxe_pickup"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.torch_use"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.ore_identified"});
  forest.addEvent({.stage = "teach", .evidence_id = "evidence.feedback_read"});
  forest.addEvent({.stage = "evaluate", .evidence_id = "evidence.feedback_read"});
  const aster::LearningRuntimeReport report =
      aster::evaluateLearningRuntime(lumenContract(), forest);
  assert(!report.passed);
}

void testMissingWorkflowEvaluation() {
  aster::LearningTraceForest forest = goodForest();
  aster::LearningRuntimeContract contract = lumenContract();
  contract.workflow_stages.push_back("reflect");
  const aster::LearningRuntimeReport report = aster::evaluateLearningRuntime(contract, forest);
  assert(!report.passed);
  assert(report.workflow_coverage < 1.0f);
}

void testLiveSessionProjectRoundTrip() {
#if defined(ASTER_SOURCE_DIR)
  const std::filesystem::path project =
      std::filesystem::path(ASTER_SOURCE_DIR) / "projects/lumen_run/lumen_run.asterproj";
#else
  const std::filesystem::path project = "projects/lumen_run/lumen_run.asterproj";
#endif
  aster::LearningSessionLoadResult loaded =
      aster::loadLearningSession(project, "lesson.lumen_mining");
  assert(loaded.ok());
  aster::LearningSession &session = loaded.session;

  session.observe({.event = "mining_attempt_without_tool",
                   .asset = "action.mine.coal_ore",
                   .channels = {"interaction.mineable", "resource.coal"},
                   .stage = "diagnose"});
  const std::vector<aster::LearningScaffoldCandidate> scaffolds = session.scaffoldCandidates();
  assert(scaffolds.size() == 1u);
  assert(scaffolds.front().scaffold_id == "scaffold.pickaxe_prompt");
  assert(session.selectScaffold(scaffolds.front().scaffold_id));

  session.observe({.event = "inventory_transfer",
                   .asset = "action.item.pickup",
                   .channels = {"interaction.pickup", "item.pickaxe"}});
  session.observe({.event = "item_use",
                   .asset = "action.item.use_torch",
                   .channels = {"item.light", "lighting_atmosphere"}});
  session.observe({.event = "focus_resource_target",
                   .asset = "scene.cave_entry",
                   .channels = {"interaction.mineable", "resource.coal", "gameplay_affordance"}});
  for (const char *event : {"mining_attempt", "surface_hit", "crack"}) {
    session.observe(
        {.event = event,
         .asset = "action.mine.coal_ore",
         .channels = {"material_memory", "event_residue", "resource_state", "ui_feedback"}});
  }
  for (const char *event : {"carve_resource_state_write", "ui_feedback"}) {
    session.observe({.event = event,
                     .asset = "action.mine.coal_ore",
                     .channels = {"resource_state", "sensory_feedback", "ui_feedback"},
                     .stage = "evaluate"});
  }
  assert(session.claimMastery());
  const aster::LearningSessionReport report = session.evaluate();
  assert(report.passed);
  assert(report.evaluators_consistent);
  assert(report.authoring.objective_coverage == 1.0f);
  assert(report.runtime.objective_coverage == 1.0f);

  const std::filesystem::path output =
      std::filesystem::temp_directory_path() / "aster_learning_session_round_trip";
  std::filesystem::remove_all(output);
  const aster::LearningSessionArtifacts artifacts = session.writeArtifacts(output);
  assert(artifacts.written);
  assert(std::filesystem::exists(artifacts.trace_path));
  assert(std::filesystem::exists(artifacts.report_path));
  const auto reparsed = aster::sdk::loadLearningTraceDocument(artifacts.trace_path);
  assert(reparsed.ok());
  assert(reparsed.value.events.size() == session.trace().events.size());
  assert(aster::sdk::evaluateLearningTrace(session.lesson(), reparsed.value).passed);
  std::filesystem::remove_all(output);
}

void testMemoryControllerReducerBudgetConflictAndReplay() {
  const std::filesystem::path store_path =
      std::filesystem::temp_directory_path() / "aster_learning_memory_controller.sqlite";
  std::filesystem::remove(store_path);

  aster::WorldState world({.fixed_step_seconds = 1.0 / 60.0});
  (void)world.appendTypedTrace({.domain = aster::TypedTraceDomain::Learning,
                                .kind = aster::TypedTraceEventKind::StateWrite,
                                .tick = 1u,
                                .subject = "entity.player",
                                .key = "lesson.lumen_mining.tool",
                                .payload = "pickaxe_missing",
                                .value_hash = 101u});

  aster::MemoryController controller(
      {.controller_id = "learning.memory.reducer",
       .store_path = store_path,
       .objective_id = "objective.memory.reducer",
       .budget = {.token_budget = 256u, .byte_budget = 4096u, .time_budget_ms = 25.0},
       .provider = {},
       .trace_window = 8u});
  const aster::MemoryControllerStepDesc write_step{
      .task = "write",
      .subject = "entity.player",
      .semantic_key = "lesson.lumen_mining.tool",
      .budget = {.token_budget = 128u, .byte_budget = 1024u, .time_budget_ms = 10.0},
      .allowed_actions = {aster::MemoryActionKind::Write, aster::MemoryActionKind::Stop}};
  const aster::MemoryDecision first = controller.step(world, write_step);
  assert(first.action == aster::MemoryActionKind::Write);
  assert(first.status == aster::MemoryDecisionStatus::Accepted);
  assert(first.decision_hash != 0u);

  (void)world.appendTypedTrace({.domain = aster::TypedTraceDomain::Learning,
                                .kind = aster::TypedTraceEventKind::StateWrite,
                                .tick = 2u,
                                .subject = "entity.player",
                                .key = "lesson.lumen_mining.tool",
                                .payload = "pickaxe_ready",
                                .value_hash = 202u});
  const aster::MemoryDecision second = controller.step(world, write_step);
  assert(second.action == aster::MemoryActionKind::Write);
  const aster::MemoryGraphQueryResult graph =
      controller.queryGraph({.store_path = store_path,
                             .subject = "entity.player",
                             .semantic_key = "lesson.lumen_mining.tool",
                             .limit = 8u});
  assert(graph.diagnostic.empty());
  assert(graph.node_count >= 1u);
  assert(graph.conflict_count >= 1u);

  const aster::MemoryDecision replay = controller.step(
      world, {.task = "resolve_conflict",
              .subject = "entity.player",
              .semantic_key = "lesson.lumen_mining.tool",
              .budget = {.token_budget = 128u, .byte_budget = 2048u, .time_budget_ms = 10.0},
              .allowed_actions = {aster::MemoryActionKind::Replay, aster::MemoryActionKind::Stop}});
  assert(replay.action == aster::MemoryActionKind::Replay);
  assert(replay.rationale.find("conflict") != std::string::npos);

  const aster::MemoryBenchmarkReport report = controller.benchmarkReport("suite.memory.reducer");
  assert(report.case_count == 3u);
  assert(report.regression_replay_count == 1u);
  assert(report.passed);
  std::filesystem::remove(store_path);
}

void testMemoryControllerBudgetedEvictionAndProviderBlock() {
  aster::WorldState world({.fixed_step_seconds = 1.0 / 60.0});
  (void)world.appendTypedTrace(
      {.domain = aster::TypedTraceDomain::Learning,
       .kind = aster::TypedTraceEventKind::StateWrite,
       .subject = "entity.player",
       .key = "lesson.lumen_mining.long_window",
       .payload = "a trace payload large enough to exceed a tiny byte budget",
       .value_hash = 303u});

  aster::MemoryController evicting(
      {.controller_id = "learning.memory.evict",
       .objective_id = "objective.memory.evict",
       .budget = {.token_budget = 64u, .byte_budget = 1u, .time_budget_ms = 5.0},
       .provider = {},
       .trace_window = 8u});
  const aster::MemoryDecision evict = evicting.step(
      world, {.task = "pressure",
              .subject = "entity.player",
              .semantic_key = "lesson.lumen_mining.long_window",
              .budget = {.token_budget = 64u, .byte_budget = 1u, .time_budget_ms = 5.0},
              .allowed_actions = {aster::MemoryActionKind::Evict, aster::MemoryActionKind::Stop}});
  assert(evict.action == aster::MemoryActionKind::Evict);
  assert(evict.saved_bytes > 0u);

  aster::MemoryProviderConfig required_provider;
  required_provider.require_provider = true;
  aster::MemoryController blocked(
      {.controller_id = "learning.memory.required_provider",
       .objective_id = "objective.memory.required_provider",
       .budget = {.token_budget = 64u, .byte_budget = 1024u, .time_budget_ms = 5.0},
       .provider = required_provider,
       .trace_window = 8u});
  const aster::MemoryDecision decision = blocked.step(
      world, {.task = "provider_required",
              .subject = "entity.player",
              .semantic_key = "lesson.lumen_mining.tool",
              .budget = {.token_budget = 64u, .byte_budget = 1024u, .time_budget_ms = 5.0},
              .allowed_actions = {aster::MemoryActionKind::Write, aster::MemoryActionKind::Stop}});
  assert(decision.status == aster::MemoryDecisionStatus::Blocked);
  assert(decision.provider_status == "provider_url_missing");
}

} // namespace

int main(int argc, char **argv) {
  const std::string test = argc > 1 ? argv[1] : "all";
  if (test == "good_lumen_trace" || test == "all") {
    testGoodLumenTrace();
  }
  if (test == "missing_tool_evidence" || test == "all") {
    testMissingToolEvidence();
  }
  if (test == "unsupported_scaffold_rationale" || test == "all") {
    testUnsupportedScaffoldRationale();
  }
  if (test == "false_mastery_rejected" || test == "all") {
    testFalseMasteryRejected();
  }
  if (test == "missing_workflow_evaluation" || test == "all") {
    testMissingWorkflowEvaluation();
  }
  if (test == "live_session_project_round_trip" || test == "all") {
    testLiveSessionProjectRoundTrip();
  }
  if (test == "memory_controller_reducer" || test == "all") {
    testMemoryControllerReducerBudgetConflictAndReplay();
  }
  if (test == "memory_controller_provider_block" || test == "all") {
    testMemoryControllerBudgetedEvictionAndProviderBlock();
  }
  std::cout << "learning_runtime_tests passed.\n";
  return 0;
}
