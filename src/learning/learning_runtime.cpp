// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/learning/learning_runtime.hpp"

#include <algorithm>
#include <set>
#include <string_view>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] bool containsString(const std::vector<std::string> &values,
                                  const std::string_view needle) {
  return std::any_of(values.begin(), values.end(),
                     [&](const std::string &value) { return value == needle; });
}

[[nodiscard]] const LearningRuntimeScaffoldRule *
findRule(const LearningRuntimeContract &contract, const std::string_view id) {
  const auto found =
      std::find_if(contract.scaffold_rules.begin(), contract.scaffold_rules.end(),
                   [&](const LearningRuntimeScaffoldRule &rule) { return rule.id == id; });
  return found == contract.scaffold_rules.end() ? nullptr : &*found;
}

void addDiagnostic(std::vector<std::string> &diagnostics, std::string diagnostic) {
  if (!diagnostic.empty() &&
      std::find(diagnostics.begin(), diagnostics.end(), diagnostic) == diagnostics.end()) {
    diagnostics.push_back(std::move(diagnostic));
  }
}

} // namespace

void LearningTraceForest::addEvent(LearningRuntimeTraceEvent event) {
  events_.push_back(std::move(event));
}

const std::vector<LearningRuntimeTraceEvent> &LearningTraceForest::events() const noexcept {
  return events_;
}

bool LearningTraceForest::observedEvidence(const std::string_view evidence_id) const {
  return std::any_of(events_.begin(), events_.end(), [&](const LearningRuntimeTraceEvent &event) {
    return event.evidence_id == evidence_id;
  });
}

bool LearningTraceForest::observedEvidenceBefore(const std::string_view evidence_id,
                                                const std::size_t event_index) const {
  const std::size_t end = std::min(event_index, events_.size());
  return std::any_of(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(end),
                     [&](const LearningRuntimeTraceEvent &event) {
                       return event.evidence_id == evidence_id;
                     });
}

bool LearningTraceForest::observedHypothesisBefore(const std::string_view hypothesis_id,
                                                  const std::size_t event_index) const {
  const std::size_t end = std::min(event_index, events_.size());
  return std::any_of(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(end),
                     [&](const LearningRuntimeTraceEvent &event) {
                       return event.hypothesis_id == hypothesis_id;
                     });
}

bool LearningTraceForest::coveredStage(const std::string_view stage) const {
  return std::any_of(events_.begin(), events_.end(), [&](const LearningRuntimeTraceEvent &event) {
    return event.stage == stage;
  });
}

std::vector<std::string> LearnerStateEstimator::hypotheses(
    const LearningTraceForest &forest) const {
  std::set<std::string> ids;
  for (const LearningRuntimeTraceEvent &event : forest.events()) {
    if (!event.hypothesis_id.empty()) {
      ids.insert(event.hypothesis_id);
    }
  }
  return {ids.begin(), ids.end()};
}

LearningRuntimeDecision EvidenceGroundingGate::evaluate(
    const LearningRuntimeScaffoldRule &rule, const LearningTraceForest &forest,
    const std::size_t event_index) const {
  LearningRuntimeDecision decision;
  decision.scaffold_id = rule.id;
  decision.stage = rule.stage;
  decision.accepted = true;
  if (rule.force_gameplay_change) {
    decision.accepted = false;
    decision.diagnostic = "scaffold forces gameplay change";
    return decision;
  }
  const LearningRuntimeTraceEvent &event = forest.events()[event_index];
  if (event.stage != rule.stage) {
    decision.accepted = false;
    decision.diagnostic = "scaffold selected in the wrong workflow stage";
    return decision;
  }
  const auto rationale = event.metadata.find("rationale");
  if (rationale == event.metadata.end() || rationale->second.empty()) {
    decision.accepted = false;
    decision.diagnostic = "scaffold rationale is missing";
    return decision;
  }
  bool rationale_grounded = false;
  for (const std::string &evidence : rule.evidence_ids) {
    if (!forest.observedEvidenceBefore(evidence, event_index)) {
      decision.accepted = false;
      decision.diagnostic = "scaffold evidence was not observed before intervention";
      return decision;
    }
    rationale_grounded = rationale_grounded || rationale->second.find(evidence) != std::string::npos;
  }
  for (const std::string &hypothesis : rule.hypothesis_ids) {
    if (!forest.observedHypothesisBefore(hypothesis, event_index)) {
      decision.accepted = false;
      decision.diagnostic = "learner-state hypothesis was not diagnosed before intervention";
      return decision;
    }
    rationale_grounded =
        rationale_grounded || rationale->second.find(hypothesis) != std::string::npos;
  }
  if (!rationale_grounded) {
    decision.accepted = false;
    decision.diagnostic = "scaffold rationale does not cite declared evidence or hypothesis";
  }
  return decision;
}

std::vector<LearningRuntimeDecision> ScaffoldPlanner::plan(
    const LearningRuntimeContract &contract, const LearningTraceForest &forest) const {
  std::vector<LearningRuntimeDecision> decisions;
  EvidenceGroundingGate gate;
  for (std::size_t index = 0u; index < forest.events().size(); ++index) {
    const LearningRuntimeTraceEvent &event = forest.events()[index];
    if (event.scaffold_id.empty()) {
      continue;
    }
    const LearningRuntimeScaffoldRule *rule = findRule(contract, event.scaffold_id);
    if (rule == nullptr) {
      decisions.push_back({.scaffold_id = event.scaffold_id,
                           .stage = event.stage,
                           .accepted = false,
                           .diagnostic = "scaffold is not declared by the lesson"});
      continue;
    }
    decisions.push_back(gate.evaluate(*rule, forest, index));
  }
  return decisions;
}

std::vector<std::string> PedagogicalSafetyGate::evaluate(
    const LearningRuntimeContract &contract, const LearningTraceForest &forest) const {
  std::vector<std::string> diagnostics;
  std::set<std::string> observed;
  const auto objectives_covered = [&]() {
    return std::all_of(contract.objectives.begin(), contract.objectives.end(),
                       [&](const LearningRuntimeObjective &objective) {
                         return std::all_of(objective.evidence_ids.begin(),
                                            objective.evidence_ids.end(),
                                            [&](const std::string &evidence) {
                                              return observed.contains(evidence);
                                            });
                       });
  };
  for (const LearningRuntimeTraceEvent &event : forest.events()) {
    if (!event.evidence_id.empty()) {
      observed.insert(event.evidence_id);
    }
    if (event.claims_mastery && !objectives_covered()) {
      addDiagnostic(diagnostics, "mastery was claimed before objectives were evidenced");
    }
  }
  for (const std::string &evidence : contract.safety_evidence_ids) {
    if (!forest.observedEvidence(evidence)) {
      addDiagnostic(diagnostics, "missing safety evidence: " + evidence);
    }
  }
  for (const LearningRuntimeScaffoldRule &rule : contract.scaffold_rules) {
    if (rule.force_gameplay_change) {
      addDiagnostic(diagnostics, "scaffold forces gameplay change: " + rule.id);
    }
  }
  return diagnostics;
}

LearningRuntimeReport evaluateLearningRuntime(const LearningRuntimeContract &contract,
                                              const LearningTraceForest &forest) {
  LearningRuntimeReport report;
  std::size_t covered_objectives = 0u;
  for (const LearningRuntimeObjective &objective : contract.objectives) {
    const bool covered =
        std::all_of(objective.evidence_ids.begin(), objective.evidence_ids.end(),
                    [&](const std::string &evidence) { return forest.observedEvidence(evidence); });
    if (covered) {
      ++covered_objectives;
    } else {
      addDiagnostic(report.diagnostics, "missing objective evidence: " + objective.id);
    }
  }
  report.objective_coverage = contract.objectives.empty()
                                  ? 0.0f
                                  : static_cast<float>(covered_objectives) /
                                        static_cast<float>(contract.objectives.size());

  std::size_t covered_stages = 0u;
  for (const std::string &stage : contract.workflow_stages) {
    if (forest.coveredStage(stage)) {
      ++covered_stages;
    } else {
      addDiagnostic(report.diagnostics, "missing workflow stage: " + stage);
    }
  }
  report.workflow_coverage = contract.workflow_stages.empty()
                                 ? 0.0f
                                 : static_cast<float>(covered_stages) /
                                       static_cast<float>(contract.workflow_stages.size());

  report.decisions = ScaffoldPlanner().plan(contract, forest);
  for (const LearningRuntimeDecision &decision : report.decisions) {
    if (!decision.accepted) {
      addDiagnostic(report.diagnostics,
                    decision.scaffold_id + ": " + decision.diagnostic);
    }
  }
  for (std::string diagnostic : PedagogicalSafetyGate().evaluate(contract, forest)) {
    addDiagnostic(report.diagnostics, std::move(diagnostic));
  }
  report.passed = report.diagnostics.empty();
  return report;
}

} // namespace aster
