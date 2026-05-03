// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace aster {

struct SceneTraceFrame {
  double time_seconds = 0.0;
  std::vector<std::string> symbols;
};

struct SceneSymbolicTrace {
  std::vector<SceneTraceFrame> frames;
};

enum class SceneTraceRuleKind {
  Forbid,
  RequireSameFrame,
  ForbidSameFrame,
  RequireWithin,
};

struct SceneTraceRule {
  std::string name;
  SceneTraceRuleKind kind = SceneTraceRuleKind::Forbid;
  std::string antecedent;
  std::string consequent;
  std::size_t horizon = 0u;
  std::size_t quantifier_rank_proxy = 1u;
};

struct SceneTraceViolation {
  std::string rule_name;
  std::size_t index = 0u;
  std::size_t horizon = 0u;
  std::string antecedent;
  std::string consequent;
};

struct SceneTraceRuleReport {
  SceneTraceRule rule;
  bool passed = true;
  std::size_t observed_horizon = 0u;
  std::vector<SceneTraceViolation> violations;
};

enum class SceneTraceDefectScale {
  None,
  Local,
  BoundedWindow,
  HorizonScale,
};

struct SceneTraceValidationReport {
  bool valid = true;
  std::size_t trace_length = 0u;
  std::size_t rank_proxy = 0u;
  SceneTraceDefectScale defect_scale = SceneTraceDefectScale::None;
  std::vector<SceneTraceRuleReport> rules;
};

struct SceneTraceSeparatorProfile {
  bool separated = false;
  std::size_t horizon = 0u;
  std::size_t rank_proxy = 0u;
  std::vector<std::string> separating_rules;
};

[[nodiscard]] bool traceFrameHasSymbol(const SceneTraceFrame &frame, const std::string &symbol);
void addTraceSymbol(SceneTraceFrame &frame, std::string symbol);

[[nodiscard]] SceneTraceRule forbidTraceSymbol(std::string name, std::string symbol);
[[nodiscard]] SceneTraceRule requireTraceSymbolSameFrame(std::string name, std::string antecedent,
                                                         std::string consequent);
[[nodiscard]] SceneTraceRule forbidTraceSymbolSameFrame(std::string name, std::string antecedent,
                                                        std::string forbidden);
[[nodiscard]] SceneTraceRule requireTraceSymbolWithin(std::string name, std::string antecedent,
                                                      std::string consequent, std::size_t horizon);

[[nodiscard]] SceneTraceValidationReport
validateSceneTrace(const SceneSymbolicTrace &trace, const std::vector<SceneTraceRule> &rules);

[[nodiscard]] SceneTraceSeparatorProfile
estimateSceneTraceSeparatorProfile(const std::vector<SceneSymbolicTrace> &accepted,
                                   const std::vector<SceneSymbolicTrace> &rejected,
                                   const std::vector<SceneTraceRule> &candidate_rules,
                                   std::size_t horizon);

[[nodiscard]] const char *sceneTraceDefectScaleName(SceneTraceDefectScale scale);

} // namespace aster
