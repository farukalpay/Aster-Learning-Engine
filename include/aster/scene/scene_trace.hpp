// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

struct SceneTraceIndistinguishablePair {
  std::size_t accepted_index = 0u;
  std::size_t rejected_index = 0u;
};

struct SceneTraceSeparatorProfile {
  bool separated = false;
  bool vacuous = false;
  bool complete_search = false;
  std::size_t horizon = 0u;
  std::size_t searched_quantifier_rank = 0u;
  std::size_t quantifier_rank = 0u;
  std::vector<SceneTraceIndistinguishablePair> indistinguishable_pairs;
};

struct SceneTraceFoSolverOptions {
  std::size_t horizon = 0u;
  std::optional<std::size_t> max_quantifier_rank{};
};

struct SceneTraceFoVariableBinding {
  std::string variable;
  std::size_t position = 0u;
};

using SceneTraceFoAssignment = std::vector<SceneTraceFoVariableBinding>;

enum class SceneTraceFoFormulaKind {
  True,
  False,
  Predicate,
  Equal,
  Less,
  Not,
  And,
  Or,
  Implies,
  Exists,
  Forall,
};

struct SceneTraceFoFormula {
  SceneTraceFoFormulaKind kind = SceneTraceFoFormulaKind::True;
  std::string predicate;
  std::string left_variable;
  std::string right_variable;
  std::shared_ptr<const SceneTraceFoFormula> left;
  std::shared_ptr<const SceneTraceFoFormula> right;
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

[[nodiscard]] SceneTraceFoFormula sceneTraceFoTrue();
[[nodiscard]] SceneTraceFoFormula sceneTraceFoFalse();
[[nodiscard]] SceneTraceFoFormula sceneTraceFoPredicate(std::string predicate,
                                                        std::string variable);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoEqual(std::string left_variable,
                                                    std::string right_variable);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoLess(std::string left_variable,
                                                   std::string right_variable);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoNot(SceneTraceFoFormula operand);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoAnd(SceneTraceFoFormula left,
                                                  SceneTraceFoFormula right);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoOr(SceneTraceFoFormula left,
                                                 SceneTraceFoFormula right);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoImplies(SceneTraceFoFormula left,
                                                      SceneTraceFoFormula right);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoExists(std::string variable,
                                                     SceneTraceFoFormula body);
[[nodiscard]] SceneTraceFoFormula sceneTraceFoForall(std::string variable,
                                                     SceneTraceFoFormula body);

[[nodiscard]] SceneTraceFoFormula parseSceneTraceFoFormula(std::string_view source);
[[nodiscard]] bool evaluateSceneTraceFoFormula(const SceneSymbolicTrace &trace,
                                               const SceneTraceFoFormula &formula);
[[nodiscard]] bool evaluateSceneTraceFoFormula(const SceneSymbolicTrace &trace,
                                               const SceneTraceFoFormula &formula,
                                               const SceneTraceFoAssignment &assignment);
[[nodiscard]] std::size_t sceneTraceFoQuantifierRank(const SceneTraceFoFormula &formula);
[[nodiscard]] std::vector<std::string> sceneTraceFoFreeVariables(
    const SceneTraceFoFormula &formula);
[[nodiscard]] bool sceneTraceFoEquivalentAtRank(const SceneSymbolicTrace &left,
                                                const SceneSymbolicTrace &right,
                                                std::size_t quantifier_rank);

[[nodiscard]] SceneTraceSeparatorProfile
solveSceneTraceFoSeparatorProfile(const std::vector<SceneSymbolicTrace> &accepted,
                                  const std::vector<SceneSymbolicTrace> &rejected,
                                  SceneTraceFoSolverOptions options = {});

[[nodiscard]] const char *sceneTraceDefectScaleName(SceneTraceDefectScale scale);

} // namespace aster
