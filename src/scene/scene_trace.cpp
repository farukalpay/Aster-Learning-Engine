// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/scene/scene_trace.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
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

enum class FoTokenKind {
  End,
  Identifier,
  String,
  LeftParen,
  RightParen,
  Dot,
  Less,
  Equal,
  Not,
  And,
  Or,
  Implies,
};

struct FoToken {
  FoTokenKind kind = FoTokenKind::End;
  std::string text;
  std::size_t offset = 0u;
};

bool isIdentifierStart(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isalpha(byte) != 0 || ch == '_';
}

bool isIdentifierContinue(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isalnum(byte) != 0 || ch == '_';
}

bool isKeyword(const std::string &text) {
  return text == "true" || text == "false" || text == "exists" || text == "forall";
}

[[noreturn]] void throwFoParseError(const std::size_t offset, const std::string &message) {
  throw std::runtime_error("Scene trace FO parse error at byte " + std::to_string(offset) + ": " +
                           message);
}

std::vector<FoToken> tokenizeFoFormula(const std::string_view source) {
  std::vector<FoToken> tokens;
  std::size_t position = 0u;
  while (position < source.size()) {
    const char ch = source[position];
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      ++position;
      continue;
    }

    const std::size_t token_offset = position;
    if (isIdentifierStart(ch)) {
      ++position;
      while (position < source.size() && isIdentifierContinue(source[position])) {
        ++position;
      }
      tokens.push_back({FoTokenKind::Identifier,
                        std::string(source.substr(token_offset, position - token_offset)),
                        token_offset});
      continue;
    }

    if (ch == '"') {
      ++position;
      std::string value;
      bool closed = false;
      while (position < source.size()) {
        const char current = source[position++];
        if (current == '"') {
          tokens.push_back({FoTokenKind::String, std::move(value), token_offset});
          closed = true;
          break;
        }
        if (current != '\\') {
          value.push_back(current);
          continue;
        }
        if (position >= source.size()) {
          throwFoParseError(token_offset, "unterminated string literal");
        }
        const char escaped = source[position++];
        switch (escaped) {
        case '"':
        case '\\':
          value.push_back(escaped);
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          throwFoParseError(position - 1u, "unsupported string escape");
        }
      }
      if (!closed) {
        throwFoParseError(token_offset, "unterminated string literal");
      }
      continue;
    }

    if (ch == '(') {
      tokens.push_back({FoTokenKind::LeftParen, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == ')') {
      tokens.push_back({FoTokenKind::RightParen, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == '.') {
      tokens.push_back({FoTokenKind::Dot, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == '<') {
      tokens.push_back({FoTokenKind::Less, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == '=') {
      tokens.push_back({FoTokenKind::Equal, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == '!') {
      tokens.push_back({FoTokenKind::Not, {}, token_offset});
      ++position;
      continue;
    }
    if (ch == '&' && position + 1u < source.size() && source[position + 1u] == '&') {
      tokens.push_back({FoTokenKind::And, {}, token_offset});
      position += 2u;
      continue;
    }
    if (ch == '|' && position + 1u < source.size() && source[position + 1u] == '|') {
      tokens.push_back({FoTokenKind::Or, {}, token_offset});
      position += 2u;
      continue;
    }
    if (ch == '-' && position + 1u < source.size() && source[position + 1u] == '>') {
      tokens.push_back({FoTokenKind::Implies, {}, token_offset});
      position += 2u;
      continue;
    }

    throwFoParseError(token_offset, "unexpected character");
  }

  tokens.push_back({FoTokenKind::End, {}, source.size()});
  return tokens;
}

class SceneTraceFoParser {
public:
  explicit SceneTraceFoParser(const std::string_view source) : tokens_(tokenizeFoFormula(source)) {}

  SceneTraceFoFormula parse() {
    SceneTraceFoFormula formula = parseImplication();
    if (peek().kind != FoTokenKind::End) {
      fail("unexpected token after formula");
    }
    return formula;
  }

private:
  const FoToken &peek() const {
    return tokens_[position_];
  }

  bool match(const FoTokenKind kind) {
    if (peek().kind != kind) {
      return false;
    }
    ++position_;
    return true;
  }

  bool matchIdentifier(const char *text) {
    if (peek().kind != FoTokenKind::Identifier || peek().text != text) {
      return false;
    }
    ++position_;
    return true;
  }

  FoToken consume(const FoTokenKind kind, const char *message) {
    if (peek().kind != kind) {
      fail(message);
    }
    return tokens_[position_++];
  }

  std::string consumeVariable() {
    if (peek().kind != FoTokenKind::Identifier) {
      fail("expected variable identifier");
    }
    if (isKeyword(peek().text)) {
      fail("keyword cannot be used as a variable");
    }
    return tokens_[position_++].text;
  }

  [[noreturn]] void fail(const std::string &message) const {
    throwFoParseError(peek().offset, message);
  }

  SceneTraceFoFormula parseImplication() {
    SceneTraceFoFormula left = parseOr();
    if (!match(FoTokenKind::Implies)) {
      return left;
    }
    return sceneTraceFoImplies(std::move(left), parseImplication());
  }

  SceneTraceFoFormula parseOr() {
    SceneTraceFoFormula formula = parseAnd();
    while (match(FoTokenKind::Or)) {
      formula = sceneTraceFoOr(std::move(formula), parseAnd());
    }
    return formula;
  }

  SceneTraceFoFormula parseAnd() {
    SceneTraceFoFormula formula = parseUnary();
    while (match(FoTokenKind::And)) {
      formula = sceneTraceFoAnd(std::move(formula), parseUnary());
    }
    return formula;
  }

  SceneTraceFoFormula parseUnary() {
    if (match(FoTokenKind::Not)) {
      return sceneTraceFoNot(parseUnary());
    }

    if (matchIdentifier("exists")) {
      const std::string variable = consumeVariable();
      consume(FoTokenKind::Dot, "expected '.' after existential variable");
      return sceneTraceFoExists(variable, parseImplication());
    }

    if (matchIdentifier("forall")) {
      const std::string variable = consumeVariable();
      consume(FoTokenKind::Dot, "expected '.' after universal variable");
      return sceneTraceFoForall(variable, parseImplication());
    }

    return parseAtom();
  }

  SceneTraceFoFormula parseAtom() {
    if (match(FoTokenKind::LeftParen)) {
      SceneTraceFoFormula formula = parseImplication();
      consume(FoTokenKind::RightParen, "expected ')' after formula");
      return formula;
    }

    if (matchIdentifier("true")) {
      return sceneTraceFoTrue();
    }
    if (matchIdentifier("false")) {
      return sceneTraceFoFalse();
    }

    if (peek().kind != FoTokenKind::Identifier && peek().kind != FoTokenKind::String) {
      fail("expected formula atom");
    }

    const FoToken name = tokens_[position_++];
    if (match(FoTokenKind::LeftParen)) {
      const std::string variable = consumeVariable();
      consume(FoTokenKind::RightParen, "expected ')' after predicate argument");
      return sceneTraceFoPredicate(name.text, variable);
    }

    if (name.kind != FoTokenKind::Identifier || isKeyword(name.text)) {
      fail("expected variable relation or predicate call");
    }
    const std::string left_variable = name.text;
    if (match(FoTokenKind::Less)) {
      return sceneTraceFoLess(left_variable, consumeVariable());
    }
    if (match(FoTokenKind::Equal)) {
      return sceneTraceFoEqual(left_variable, consumeVariable());
    }
    fail("expected predicate call, '<', or '='");
  }

  std::vector<FoToken> tokens_;
  std::size_t position_ = 0u;
};

std::shared_ptr<const SceneTraceFoFormula> makeFormulaPtr(SceneTraceFoFormula formula) {
  return std::make_shared<SceneTraceFoFormula>(std::move(formula));
}

const SceneTraceFoFormula &requireFoChild(
    const std::shared_ptr<const SceneTraceFoFormula> &child) {
  if (!child) {
    throw std::invalid_argument("Malformed scene trace FO formula.");
  }
  return *child;
}

std::size_t resolveFoVariable(const std::unordered_map<std::string, std::size_t> &assignment,
                              const SceneSymbolicTrace &trace, const std::string &variable) {
  const auto found = assignment.find(variable);
  if (found == assignment.end()) {
    throw std::invalid_argument("Unbound scene trace FO variable: " + variable);
  }
  if (found->second >= trace.frames.size()) {
    throw std::invalid_argument("Scene trace FO variable is outside the trace domain: " +
                                variable);
  }
  return found->second;
}

bool evaluateFoFormula(const SceneSymbolicTrace &trace, const SceneTraceFoFormula &formula,
                       std::unordered_map<std::string, std::size_t> &assignment) {
  switch (formula.kind) {
  case SceneTraceFoFormulaKind::True:
    return true;
  case SceneTraceFoFormulaKind::False:
    return false;
  case SceneTraceFoFormulaKind::Predicate:
    return traceFrameHasSymbol(trace.frames[resolveFoVariable(assignment, trace,
                                                              formula.left_variable)],
                               formula.predicate);
  case SceneTraceFoFormulaKind::Equal:
    return resolveFoVariable(assignment, trace, formula.left_variable) ==
           resolveFoVariable(assignment, trace, formula.right_variable);
  case SceneTraceFoFormulaKind::Less:
    return resolveFoVariable(assignment, trace, formula.left_variable) <
           resolveFoVariable(assignment, trace, formula.right_variable);
  case SceneTraceFoFormulaKind::Not:
    return !evaluateFoFormula(trace, requireFoChild(formula.left), assignment);
  case SceneTraceFoFormulaKind::And:
    return evaluateFoFormula(trace, requireFoChild(formula.left), assignment) &&
           evaluateFoFormula(trace, requireFoChild(formula.right), assignment);
  case SceneTraceFoFormulaKind::Or:
    return evaluateFoFormula(trace, requireFoChild(formula.left), assignment) ||
           evaluateFoFormula(trace, requireFoChild(formula.right), assignment);
  case SceneTraceFoFormulaKind::Implies:
    return !evaluateFoFormula(trace, requireFoChild(formula.left), assignment) ||
           evaluateFoFormula(trace, requireFoChild(formula.right), assignment);
  case SceneTraceFoFormulaKind::Exists: {
    const auto previous = assignment.find(formula.left_variable);
    const bool had_previous = previous != assignment.end();
    const std::size_t previous_value = had_previous ? previous->second : 0u;
    for (std::size_t i = 0u; i < trace.frames.size(); ++i) {
      assignment[formula.left_variable] = i;
      if (evaluateFoFormula(trace, requireFoChild(formula.left), assignment)) {
        if (had_previous) {
          assignment[formula.left_variable] = previous_value;
        } else {
          assignment.erase(formula.left_variable);
        }
        return true;
      }
    }
    if (had_previous) {
      assignment[formula.left_variable] = previous_value;
    } else {
      assignment.erase(formula.left_variable);
    }
    return false;
  }
  case SceneTraceFoFormulaKind::Forall: {
    const auto previous = assignment.find(formula.left_variable);
    const bool had_previous = previous != assignment.end();
    const std::size_t previous_value = had_previous ? previous->second : 0u;
    for (std::size_t i = 0u; i < trace.frames.size(); ++i) {
      assignment[formula.left_variable] = i;
      if (!evaluateFoFormula(trace, requireFoChild(formula.left), assignment)) {
        if (had_previous) {
          assignment[formula.left_variable] = previous_value;
        } else {
          assignment.erase(formula.left_variable);
        }
        return false;
      }
    }
    if (had_previous) {
      assignment[formula.left_variable] = previous_value;
    } else {
      assignment.erase(formula.left_variable);
    }
    return true;
  }
  }
  return false;
}

std::size_t quantifierRankOf(const SceneTraceFoFormula &formula) {
  switch (formula.kind) {
  case SceneTraceFoFormulaKind::True:
  case SceneTraceFoFormulaKind::False:
  case SceneTraceFoFormulaKind::Predicate:
  case SceneTraceFoFormulaKind::Equal:
  case SceneTraceFoFormulaKind::Less:
    return 0u;
  case SceneTraceFoFormulaKind::Not:
    return quantifierRankOf(requireFoChild(formula.left));
  case SceneTraceFoFormulaKind::And:
  case SceneTraceFoFormulaKind::Or:
  case SceneTraceFoFormulaKind::Implies:
    return std::max(quantifierRankOf(requireFoChild(formula.left)),
                    quantifierRankOf(requireFoChild(formula.right)));
  case SceneTraceFoFormulaKind::Exists:
  case SceneTraceFoFormulaKind::Forall:
    return 1u + quantifierRankOf(requireFoChild(formula.left));
  }
  return 0u;
}

bool containsString(const std::vector<std::string> &values, const std::string &value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void addFreeVariable(const std::string &variable, const std::vector<std::string> &bound,
                     std::vector<std::string> &free) {
  if (!containsString(bound, variable) && !containsString(free, variable)) {
    free.push_back(variable);
  }
}

void collectFreeVariables(const SceneTraceFoFormula &formula, std::vector<std::string> &bound,
                          std::vector<std::string> &free) {
  switch (formula.kind) {
  case SceneTraceFoFormulaKind::True:
  case SceneTraceFoFormulaKind::False:
    return;
  case SceneTraceFoFormulaKind::Predicate:
    addFreeVariable(formula.left_variable, bound, free);
    return;
  case SceneTraceFoFormulaKind::Equal:
  case SceneTraceFoFormulaKind::Less:
    addFreeVariable(formula.left_variable, bound, free);
    addFreeVariable(formula.right_variable, bound, free);
    return;
  case SceneTraceFoFormulaKind::Not:
    collectFreeVariables(requireFoChild(formula.left), bound, free);
    return;
  case SceneTraceFoFormulaKind::And:
  case SceneTraceFoFormulaKind::Or:
  case SceneTraceFoFormulaKind::Implies:
    collectFreeVariables(requireFoChild(formula.left), bound, free);
    collectFreeVariables(requireFoChild(formula.right), bound, free);
    return;
  case SceneTraceFoFormulaKind::Exists:
  case SceneTraceFoFormulaKind::Forall:
    bound.push_back(formula.left_variable);
    collectFreeVariables(requireFoChild(formula.left), bound, free);
    bound.pop_back();
    return;
  }
}

bool framesHaveSameSymbolSet(const SceneTraceFrame &left, const SceneTraceFrame &right) {
  for (const std::string &symbol : left.symbols) {
    if (!traceFrameHasSymbol(right, symbol)) {
      return false;
    }
  }
  for (const std::string &symbol : right.symbols) {
    if (!traceFrameHasSymbol(left, symbol)) {
      return false;
    }
  }
  return true;
}

class SceneTraceEfSolver {
public:
  SceneTraceEfSolver(const SceneSymbolicTrace &left, const SceneSymbolicTrace &right)
      : left_(left), right_(right) {}

  bool duplicatorWins(const std::size_t remaining_rank) {
    pebbles_.clear();
    memo_.clear();
    return duplicatorWinsState(remaining_rank);
  }

private:
  bool partialIsomorphismHolds() const {
    for (std::size_t i = 0u; i < pebbles_.size(); ++i) {
      const auto [left_i, right_i] = pebbles_[i];
      if (!framesHaveSameSymbolSet(left_.frames[left_i], right_.frames[right_i])) {
        return false;
      }
      for (std::size_t j = 0u; j < pebbles_.size(); ++j) {
        const auto [left_j, right_j] = pebbles_[j];
        if ((left_i == left_j) != (right_i == right_j)) {
          return false;
        }
        if ((left_i < left_j) != (right_i < right_j)) {
          return false;
        }
      }
    }
    return true;
  }

  std::string memoKey(const std::size_t remaining_rank) const {
    std::string key = std::to_string(remaining_rank);
    for (const auto [left_index, right_index] : pebbles_) {
      key.push_back('|');
      key += std::to_string(left_index);
      key.push_back(',');
      key += std::to_string(right_index);
    }
    return key;
  }

  bool duplicatorWinsState(const std::size_t remaining_rank) {
    const std::string key = memoKey(remaining_rank);
    const auto cached = memo_.find(key);
    if (cached != memo_.end()) {
      return cached->second;
    }

    if (!partialIsomorphismHolds()) {
      memo_[key] = false;
      return false;
    }
    if (remaining_rank == 0u) {
      memo_[key] = true;
      return true;
    }

    for (std::size_t left_choice = 0u; left_choice < left_.frames.size(); ++left_choice) {
      bool has_response = false;
      for (std::size_t right_response = 0u; right_response < right_.frames.size();
           ++right_response) {
        pebbles_.push_back({left_choice, right_response});
        has_response = duplicatorWinsState(remaining_rank - 1u);
        pebbles_.pop_back();
        if (has_response) {
          break;
        }
      }
      if (!has_response) {
        memo_[key] = false;
        return false;
      }
    }

    for (std::size_t right_choice = 0u; right_choice < right_.frames.size(); ++right_choice) {
      bool has_response = false;
      for (std::size_t left_response = 0u; left_response < left_.frames.size();
           ++left_response) {
        pebbles_.push_back({left_response, right_choice});
        has_response = duplicatorWinsState(remaining_rank - 1u);
        pebbles_.pop_back();
        if (has_response) {
          break;
        }
      }
      if (!has_response) {
        memo_[key] = false;
        return false;
      }
    }

    memo_[key] = true;
    return true;
  }

  const SceneSymbolicTrace &left_;
  const SceneSymbolicTrace &right_;
  std::vector<std::pair<std::size_t, std::size_t>> pebbles_;
  std::unordered_map<std::string, bool> memo_;
};

std::size_t completeFoRankBound(const std::vector<SceneSymbolicTrace> &accepted,
                                const std::vector<SceneSymbolicTrace> &rejected) {
  std::size_t bound = 0u;
  for (const SceneSymbolicTrace &trace : accepted) {
    bound = std::max(bound, trace.frames.size());
  }
  for (const SceneSymbolicTrace &trace : rejected) {
    bound = std::max(bound, trace.frames.size());
  }
  return bound;
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

SceneTraceFoFormula sceneTraceFoTrue() {
  return {.kind = SceneTraceFoFormulaKind::True};
}

SceneTraceFoFormula sceneTraceFoFalse() {
  return {.kind = SceneTraceFoFormulaKind::False};
}

SceneTraceFoFormula sceneTraceFoPredicate(std::string predicate, std::string variable) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Predicate;
  formula.predicate = std::move(predicate);
  formula.left_variable = std::move(variable);
  return formula;
}

SceneTraceFoFormula sceneTraceFoEqual(std::string left_variable, std::string right_variable) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Equal;
  formula.left_variable = std::move(left_variable);
  formula.right_variable = std::move(right_variable);
  return formula;
}

SceneTraceFoFormula sceneTraceFoLess(std::string left_variable, std::string right_variable) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Less;
  formula.left_variable = std::move(left_variable);
  formula.right_variable = std::move(right_variable);
  return formula;
}

SceneTraceFoFormula sceneTraceFoNot(SceneTraceFoFormula operand) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Not;
  formula.left = makeFormulaPtr(std::move(operand));
  return formula;
}

SceneTraceFoFormula sceneTraceFoAnd(SceneTraceFoFormula left, SceneTraceFoFormula right) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::And;
  formula.left = makeFormulaPtr(std::move(left));
  formula.right = makeFormulaPtr(std::move(right));
  return formula;
}

SceneTraceFoFormula sceneTraceFoOr(SceneTraceFoFormula left, SceneTraceFoFormula right) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Or;
  formula.left = makeFormulaPtr(std::move(left));
  formula.right = makeFormulaPtr(std::move(right));
  return formula;
}

SceneTraceFoFormula sceneTraceFoImplies(SceneTraceFoFormula left, SceneTraceFoFormula right) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Implies;
  formula.left = makeFormulaPtr(std::move(left));
  formula.right = makeFormulaPtr(std::move(right));
  return formula;
}

SceneTraceFoFormula sceneTraceFoExists(std::string variable, SceneTraceFoFormula body) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Exists;
  formula.left_variable = std::move(variable);
  formula.left = makeFormulaPtr(std::move(body));
  return formula;
}

SceneTraceFoFormula sceneTraceFoForall(std::string variable, SceneTraceFoFormula body) {
  SceneTraceFoFormula formula;
  formula.kind = SceneTraceFoFormulaKind::Forall;
  formula.left_variable = std::move(variable);
  formula.left = makeFormulaPtr(std::move(body));
  return formula;
}

SceneTraceFoFormula parseSceneTraceFoFormula(const std::string_view source) {
  return SceneTraceFoParser(source).parse();
}

bool evaluateSceneTraceFoFormula(const SceneSymbolicTrace &trace,
                                 const SceneTraceFoFormula &formula) {
  return evaluateSceneTraceFoFormula(trace, formula, {});
}

bool evaluateSceneTraceFoFormula(const SceneSymbolicTrace &trace,
                                 const SceneTraceFoFormula &formula,
                                 const SceneTraceFoAssignment &assignment) {
  std::unordered_map<std::string, std::size_t> assignment_map;
  for (const SceneTraceFoVariableBinding &binding : assignment) {
    assignment_map[binding.variable] = binding.position;
  }
  return evaluateFoFormula(trace, formula, assignment_map);
}

std::size_t sceneTraceFoQuantifierRank(const SceneTraceFoFormula &formula) {
  return quantifierRankOf(formula);
}

std::vector<std::string> sceneTraceFoFreeVariables(const SceneTraceFoFormula &formula) {
  std::vector<std::string> bound;
  std::vector<std::string> free;
  collectFreeVariables(formula, bound, free);
  return free;
}

bool sceneTraceFoEquivalentAtRank(const SceneSymbolicTrace &left,
                                  const SceneSymbolicTrace &right,
                                  const std::size_t quantifier_rank) {
  SceneTraceEfSolver solver(left, right);
  return solver.duplicatorWins(quantifier_rank);
}

SceneTraceSeparatorProfile
solveSceneTraceFoSeparatorProfile(const std::vector<SceneSymbolicTrace> &accepted,
                                  const std::vector<SceneSymbolicTrace> &rejected,
                                  const SceneTraceFoSolverOptions options) {
  SceneTraceSeparatorProfile profile;
  profile.horizon = options.horizon;

  std::vector<SceneSymbolicTrace> accepted_traces;
  std::vector<SceneSymbolicTrace> rejected_traces;
  accepted_traces.reserve(accepted.size());
  rejected_traces.reserve(rejected.size());
  for (const SceneSymbolicTrace &trace : accepted) {
    accepted_traces.push_back(truncatedTrace(trace, options.horizon));
  }
  for (const SceneSymbolicTrace &trace : rejected) {
    rejected_traces.push_back(truncatedTrace(trace, options.horizon));
  }

  const std::size_t complete_bound = completeFoRankBound(accepted_traces, rejected_traces);
  const std::size_t rank_bound = options.max_quantifier_rank.value_or(complete_bound);
  profile.complete_search = !options.max_quantifier_rank.has_value() || rank_bound >= complete_bound;
  profile.searched_quantifier_rank = rank_bound;

  if (accepted_traces.empty() || rejected_traces.empty()) {
    profile.separated = true;
    profile.vacuous = true;
    profile.complete_search = true;
    profile.quantifier_rank = 0u;
    profile.searched_quantifier_rank = 0u;
    return profile;
  }

  for (std::size_t rank = 0u; rank <= rank_bound; ++rank) {
    std::vector<SceneTraceIndistinguishablePair> indistinguishable_pairs;
    for (std::size_t accepted_index = 0u; accepted_index < accepted_traces.size();
         ++accepted_index) {
      for (std::size_t rejected_index = 0u; rejected_index < rejected_traces.size();
           ++rejected_index) {
        if (sceneTraceFoEquivalentAtRank(accepted_traces[accepted_index],
                                         rejected_traces[rejected_index], rank)) {
          indistinguishable_pairs.push_back({accepted_index, rejected_index});
        }
      }
    }
    if (indistinguishable_pairs.empty()) {
      profile.separated = true;
      profile.quantifier_rank = rank;
      profile.searched_quantifier_rank = rank;
      return profile;
    }
    profile.indistinguishable_pairs = std::move(indistinguishable_pairs);
  }

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
