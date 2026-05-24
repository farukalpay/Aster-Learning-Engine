// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/learning/learning_runtime.hpp"

#include <cassert>
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
  forest.addEvent({.id = "t2",
                   .stage = "design",
                   .scaffold_id = "scaffold.pickaxe_prompt",
                   .metadata = {{"rationale", "evidence.mine_attempt supports hypothesis.tool_affordance_gap"}}});
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
  forest.addEvent({.stage = "design",
                   .scaffold_id = "scaffold.pickaxe_prompt",
                   .metadata = {{"rationale", "evidence.mine_attempt supports hypothesis.tool_affordance_gap"}}});
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
  std::cout << "learning_runtime_tests passed.\n";
  return 0;
}
