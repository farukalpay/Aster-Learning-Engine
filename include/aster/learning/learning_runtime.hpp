// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aster {

struct LearningRuntimeObjective {
  std::string id;
  std::vector<std::string> evidence_ids;
};

struct LearningRuntimeScaffoldRule {
  std::string id;
  std::string stage;
  std::vector<std::string> evidence_ids;
  std::vector<std::string> hypothesis_ids;
  bool force_gameplay_change = false;
};

struct LearningRuntimeContract {
  std::string lesson_id;
  std::vector<std::string> workflow_stages;
  std::vector<std::string> safety_evidence_ids;
  std::vector<LearningRuntimeObjective> objectives;
  std::vector<LearningRuntimeScaffoldRule> scaffold_rules;
};

struct LearningRuntimeTraceEvent {
  std::string id;
  std::string stage;
  std::string evidence_id;
  std::string hypothesis_id;
  std::string scaffold_id;
  std::map<std::string, std::string> metadata;
  bool claims_mastery = false;
};

struct LearningRuntimeDecision {
  std::string scaffold_id;
  std::string stage;
  bool accepted = false;
  std::string diagnostic;
};

struct LearningRuntimeReport {
  bool passed = false;
  float objective_coverage = 0.0f;
  float workflow_coverage = 0.0f;
  std::vector<LearningRuntimeDecision> decisions;
  std::vector<std::string> diagnostics;
};

class LearningTraceForest {
public:
  void addEvent(LearningRuntimeTraceEvent event);

  [[nodiscard]] const std::vector<LearningRuntimeTraceEvent> &events() const noexcept;
  [[nodiscard]] bool observedEvidence(std::string_view evidence_id) const;
  [[nodiscard]] bool observedEvidenceBefore(std::string_view evidence_id,
                                            std::size_t event_index) const;
  [[nodiscard]] bool observedHypothesisBefore(std::string_view hypothesis_id,
                                              std::size_t event_index) const;
  [[nodiscard]] bool coveredStage(std::string_view stage) const;

private:
  std::vector<LearningRuntimeTraceEvent> events_;
};

class LearnerStateEstimator {
public:
  [[nodiscard]] std::vector<std::string> hypotheses(const LearningTraceForest &forest) const;
};

class EvidenceGroundingGate {
public:
  [[nodiscard]] LearningRuntimeDecision evaluate(const LearningRuntimeScaffoldRule &rule,
                                                 const LearningTraceForest &forest,
                                                 std::size_t event_index) const;
};

class ScaffoldPlanner {
public:
  [[nodiscard]] std::vector<LearningRuntimeDecision>
  plan(const LearningRuntimeContract &contract, const LearningTraceForest &forest) const;
};

class PedagogicalSafetyGate {
public:
  [[nodiscard]] std::vector<std::string> evaluate(const LearningRuntimeContract &contract,
                                                  const LearningTraceForest &forest) const;
};

[[nodiscard]] LearningRuntimeReport
evaluateLearningRuntime(const LearningRuntimeContract &contract, const LearningTraceForest &forest);

} // namespace aster
