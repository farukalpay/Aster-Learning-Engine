// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/scene_trace.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace aster {
namespace {

bool hasSymbolInFrame(const SceneSymbolicTrace &trace, const std::size_t index,
                      const std::string &symbol) {
  return index < trace.frames.size() && traceFrameHasSymbol(trace.frames[index], symbol);
}

SceneSymbolicTrace truncatedTrace(const SceneSymbolicTrace &trace, const std::size_t horizon) {
  SceneSymbolicTrace truncated;
  const std::size_t count =
      horizon == 0u ? trace.frames.size() : std::min(trace.frames.size(), horizon);
  truncated.frames.insert(truncated.frames.end(), trace.frames.begin(),
                          trace.frames.begin() + static_cast<std::ptrdiff_t>(count));
  return truncated;
}

SceneTraceDefectScale classifyDefectScale(const std::size_t observed_horizon,
                                          const std::size_t trace_length) {
  if (observed_horizon == 0u) {
    return SceneTraceDefectScale::None;
  }
  if (observed_horizon <= 1u) {
    return SceneTraceDefectScale::Local;
  }
  if (trace_length > 1u && observed_horizon >= trace_length - 1u) {
    return SceneTraceDefectScale::HorizonScale;
  }
  return SceneTraceDefectScale::BoundedWindow;
}

void mergeDefectScale(SceneTraceValidationReport &report, const std::size_t observed_horizon) {
  const SceneTraceDefectScale scale = classifyDefectScale(observed_horizon, report.trace_length);
  if (static_cast<int>(scale) > static_cast<int>(report.defect_scale)) {
    report.defect_scale = scale;
  }
}

void evaluateForbidRule(const SceneSymbolicTrace &trace, const SceneTraceRule &rule,
                        SceneTraceRuleReport &rule_report) {
  for (std::size_t i = 0u; i < trace.frames.size(); ++i) {
    if (!traceFrameHasSymbol(trace.frames[i], rule.antecedent)) {
      continue;
    }
    rule_report.passed = false;
    rule_report.observed_horizon = std::max(rule_report.observed_horizon, std::size_t{1u});
    rule_report.violations.push_back({rule.name, i, 1u, rule.antecedent, std::string{}});
  }
}

void evaluateSameFrameRule(const SceneSymbolicTrace &trace, const SceneTraceRule &rule,
                           SceneTraceRuleReport &rule_report) {
  for (std::size_t i = 0u; i < trace.frames.size(); ++i) {
    if (!traceFrameHasSymbol(trace.frames[i], rule.antecedent)) {
      continue;
    }
    const bool has_consequent = traceFrameHasSymbol(trace.frames[i], rule.consequent);
    const bool failed =
        rule.kind == SceneTraceRuleKind::RequireSameFrame ? !has_consequent : has_consequent;
    if (!failed) {
      continue;
    }
    rule_report.passed = false;
    rule_report.observed_horizon = std::max(rule_report.observed_horizon, std::size_t{1u});
    rule_report.violations.push_back({rule.name, i, 1u, rule.antecedent, rule.consequent});
  }
}

void evaluateWithinRule(const SceneSymbolicTrace &trace, const SceneTraceRule &rule,
                        SceneTraceRuleReport &rule_report) {
  for (std::size_t i = 0u; i < trace.frames.size(); ++i) {
    if (!traceFrameHasSymbol(trace.frames[i], rule.antecedent)) {
      continue;
    }
    const std::size_t end = std::min(trace.frames.size() - 1u, i + rule.horizon);
    bool found = false;
    std::size_t witness_horizon = rule.horizon;
    for (std::size_t j = i; j <= end; ++j) {
      if (!hasSymbolInFrame(trace, j, rule.consequent)) {
        continue;
      }
      found = true;
      witness_horizon = j - i;
      break;
    }
    rule_report.observed_horizon = std::max(rule_report.observed_horizon, witness_horizon);
    if (found) {
      continue;
    }
    rule_report.passed = false;
    rule_report.violations.push_back(
        {rule.name, i, rule.horizon, rule.antecedent, rule.consequent});
  }
}

} // namespace

bool traceFrameHasSymbol(const SceneTraceFrame &frame, const std::string &symbol) {
  return std::find(frame.symbols.begin(), frame.symbols.end(), symbol) != frame.symbols.end();
}

void addTraceSymbol(SceneTraceFrame &frame, std::string symbol) {
  if (!traceFrameHasSymbol(frame, symbol)) {
    frame.symbols.push_back(std::move(symbol));
  }
}

SceneTraceRule forbidTraceSymbol(std::string name, std::string symbol) {
  return {std::move(name), SceneTraceRuleKind::Forbid, std::move(symbol), {}, 0u, 1u};
}

SceneTraceRule requireTraceSymbolSameFrame(std::string name, std::string antecedent,
                                           std::string consequent) {
  return {std::move(name),
          SceneTraceRuleKind::RequireSameFrame,
          std::move(antecedent),
          std::move(consequent),
          0u,
          1u};
}

SceneTraceRule forbidTraceSymbolSameFrame(std::string name, std::string antecedent,
                                          std::string forbidden) {
  return {std::move(name),
          SceneTraceRuleKind::ForbidSameFrame,
          std::move(antecedent),
          std::move(forbidden),
          0u,
          1u};
}

SceneTraceRule requireTraceSymbolWithin(std::string name, std::string antecedent,
                                        std::string consequent, const std::size_t horizon) {
  return {std::move(name),
          SceneTraceRuleKind::RequireWithin,
          std::move(antecedent),
          std::move(consequent),
          horizon,
          2u};
}

SceneTraceValidationReport validateSceneTrace(const SceneSymbolicTrace &trace,
                                              const std::vector<SceneTraceRule> &rules) {
  SceneTraceValidationReport report;
  report.trace_length = trace.frames.size();
  report.rules.reserve(rules.size());

  for (const SceneTraceRule &rule : rules) {
    SceneTraceRuleReport rule_report;
    rule_report.rule = rule;
    switch (rule.kind) {
    case SceneTraceRuleKind::Forbid:
      evaluateForbidRule(trace, rule, rule_report);
      break;
    case SceneTraceRuleKind::RequireSameFrame:
    case SceneTraceRuleKind::ForbidSameFrame:
      evaluateSameFrameRule(trace, rule, rule_report);
      break;
    case SceneTraceRuleKind::RequireWithin:
      evaluateWithinRule(trace, rule, rule_report);
      break;
    }

    if (!rule_report.passed) {
      report.valid = false;
      report.rank_proxy = std::max(report.rank_proxy, rule.quantifier_rank_proxy);
      mergeDefectScale(report, rule_report.observed_horizon);
    }
    report.rules.push_back(std::move(rule_report));
  }

  return report;
}

SceneTraceSeparatorProfile
estimateSceneTraceSeparatorProfile(const std::vector<SceneSymbolicTrace> &accepted,
                                   const std::vector<SceneSymbolicTrace> &rejected,
                                   const std::vector<SceneTraceRule> &candidate_rules,
                                   const std::size_t horizon) {
  SceneTraceSeparatorProfile profile;
  profile.horizon = horizon;
  if (accepted.empty() || rejected.empty()) {
    return profile;
  }
  std::size_t best_rank = std::numeric_limits<std::size_t>::max();

  for (const SceneTraceRule &rule : candidate_rules) {
    const std::vector<SceneTraceRule> single_rule{rule};
    bool accepts_all = true;
    for (const SceneSymbolicTrace &trace : accepted) {
      accepts_all =
          accepts_all && validateSceneTrace(truncatedTrace(trace, horizon), single_rule).valid;
    }
    if (!accepts_all) {
      continue;
    }

    bool rejects_all = true;
    for (const SceneSymbolicTrace &trace : rejected) {
      rejects_all =
          rejects_all && !validateSceneTrace(truncatedTrace(trace, horizon), single_rule).valid;
    }
    if (!rejects_all) {
      continue;
    }

    if (rule.quantifier_rank_proxy < best_rank) {
      best_rank = rule.quantifier_rank_proxy;
      profile.separating_rules.clear();
    }
    if (rule.quantifier_rank_proxy == best_rank) {
      profile.separating_rules.push_back(rule.name);
    }
  }

  profile.separated = best_rank != std::numeric_limits<std::size_t>::max();
  profile.rank_proxy = profile.separated ? best_rank : 0u;
  return profile;
}

const char *sceneTraceDefectScaleName(const SceneTraceDefectScale scale) {
  switch (scale) {
  case SceneTraceDefectScale::None:
    return "none";
  case SceneTraceDefectScale::Local:
    return "local";
  case SceneTraceDefectScale::BoundedWindow:
    return "bounded_window";
  case SceneTraceDefectScale::HorizonScale:
    return "horizon_scale";
  }
  return "unknown";
}

} // namespace aster
