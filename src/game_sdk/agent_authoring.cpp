// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/game_sdk/agent_authoring.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace aster::sdk {
namespace {

[[nodiscard]] std::string lowerSnake(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    if (c == '-' || c == ' ') {
      out.push_back('_');
      continue;
    }
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

[[nodiscard]] std::string trimCopy(const std::string_view value) {
  const auto is_space = [](const unsigned char c) {
    return std::isspace(c) != 0;
  };
  std::size_t first = 0u;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) {
    ++first;
  }
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1u]))) {
    --last;
  }
  return std::string(value.substr(first, last - first));
}

[[nodiscard]] std::string pathText(const std::filesystem::path &path) {
  const std::string text = path.generic_string();
  return text.empty() ? "." : text;
}

[[nodiscard]] bool pathWithinRoot(const std::filesystem::path &path,
                                  const std::filesystem::path &root) {
  if (root.empty()) {
    return true;
  }
  const std::filesystem::path relative = path.lexically_relative(root);
  if (relative.empty()) {
    return path.lexically_normal() == root.lexically_normal();
  }
  for (const std::filesystem::path &part : relative) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::filesystem::path absoluteLexical(std::filesystem::path path) {
  std::error_code error;
  std::filesystem::path absolute = std::filesystem::absolute(path, error);
  if (error) {
    return path.lexically_normal();
  }
  return absolute.lexically_normal();
}

[[nodiscard]] std::filesystem::path inferWorkspaceRoot(const std::filesystem::path &project_root) {
  std::error_code error;
  std::filesystem::path current = std::filesystem::current_path(error);
  if (error) {
    return project_root.lexically_normal();
  }
  current = current.lexically_normal();
  const std::filesystem::path absolute_project_root = absoluteLexical(project_root);
  if (pathWithinRoot(absolute_project_root, current)) {
    return current;
  }
  return project_root.lexically_normal();
}

[[nodiscard]] std::string shellQuotePath(const std::filesystem::path &path) {
  const std::string text = pathText(path);
  if (text.find_first_of(" \t'\"()[]{}") == std::string::npos) {
    return text;
  }
  std::string out = "'";
  for (const char c : text) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

[[nodiscard]] std::string escapeJson(const std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8u);
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

[[nodiscard]] std::uint64_t stableStringStamp(const std::string_view value,
                                              std::uint64_t seed = 1469598103934665603ull) {
  for (const char c : value) {
    seed ^= static_cast<unsigned char>(c);
    seed *= 1099511628211ull;
  }
  return seed;
}

[[nodiscard]] std::uint64_t hashCombine(std::uint64_t seed, const std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
  seed ^= seed >> 33u;
  seed *= 0xff51afd7ed558ccdull;
  seed ^= seed >> 33u;
  seed *= 0xc4ceb9fe1a85ec53ull;
  seed ^= seed >> 33u;
  return seed;
}

[[nodiscard]] bool containsDomain(const std::vector<AsterAgentDomain> &domains,
                                  const AsterAgentDomain domain) {
  return std::find(domains.begin(), domains.end(), domain) != domains.end();
}

void mergeDomain(std::vector<AsterAgentDomain> &domains, const AsterAgentDomain domain) {
  if (domain != AsterAgentDomain::Unknown && !containsDomain(domains, domain)) {
    domains.push_back(domain);
  }
}

void mergeDomains(std::vector<AsterAgentDomain> &domains,
                  const std::vector<AsterAgentDomain> &incoming) {
  for (const AsterAgentDomain domain : incoming) {
    mergeDomain(domains, domain);
  }
}

[[nodiscard]] bool pathEscapesRoot(const std::filesystem::path &path) {
  if (path.is_absolute()) {
    return false;
  }
  for (const std::filesystem::path &part : path) {
    if (part == "..") {
      return true;
    }
  }
  return false;
}

void addDiagnostic(std::vector<Diagnostic> &diagnostics, DiagnosticSeverity severity,
                   std::filesystem::path source, std::string path, std::string message) {
  diagnostics.push_back({severity, std::move(source), std::move(path), std::move(message)});
}

[[nodiscard]] std::vector<std::filesystem::path>
assetPathsFor(const ProjectDocument &project, const std::set<AssetKind> &kinds) {
  std::vector<std::filesystem::path> paths;
  for (const ProjectAssetRef &asset : project.assets) {
    if (kinds.find(asset.kind) != kinds.end()) {
      paths.push_back(asset.path);
    }
  }
  std::sort(paths.begin(), paths.end());
  paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
  return paths;
}

[[nodiscard]] bool hasAssetKind(const ProjectDocument &project, const std::set<AssetKind> &kinds) {
  return std::any_of(project.assets.begin(), project.assets.end(), [&](const ProjectAssetRef &asset) {
    return kinds.find(asset.kind) != kinds.end();
  });
}

[[nodiscard]] std::string domainListText(const std::vector<AsterAgentDomain> &domains) {
  std::ostringstream out;
  for (std::size_t i = 0u; i < domains.size(); ++i) {
    if (i > 0u) {
      out << ", ";
    }
    out << asterAgentDomainName(domains[i]);
  }
  return out.str();
}

[[nodiscard]] std::vector<std::filesystem::path>
scopePathsForDomain(const AsterAgentWorkspaceProfile &profile, const AsterAgentDomain domain) {
  std::vector<std::filesystem::path> paths;
  for (const AsterAgentScopeRule &scope : profile.scopes) {
    if (containsDomain(scope.domains, domain)) {
      paths.push_back(scope.path);
    }
  }
  std::sort(paths.begin(), paths.end());
  paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
  return paths;
}

[[nodiscard]] std::string outputContractText(const AsterAgentOutputContract &contract) {
  std::ostringstream out;
  out << contract.id << ": " << contract.description;
  if (!contract.required_keys.empty()) {
    out << " [";
    for (std::size_t i = 0u; i < contract.required_keys.size(); ++i) {
      if (i > 0u) {
        out << ", ";
      }
      out << contract.required_keys[i];
    }
    out << "]";
  }
  return out.str();
}

[[nodiscard]] bool containsString(const std::vector<std::string> &values,
                                  const std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

[[nodiscard]] bool startsWith(const std::string_view value, const std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0u, prefix.size()) == prefix;
}

[[nodiscard]] bool isNumberedRule(const std::string_view value) {
  std::size_t pos = 0u;
  while (pos < value.size() && std::isdigit(static_cast<unsigned char>(value[pos])) != 0) {
    ++pos;
  }
  return pos > 0u && pos + 1u < value.size() && value[pos] == '.' &&
         std::isspace(static_cast<unsigned char>(value[pos + 1u])) != 0;
}

[[nodiscard]] std::vector<std::string> instructionRulesFromText(
    const std::string_view text, const std::uint32_t max_rule_chars) {
  std::vector<std::string> rules;
  std::istringstream lines{std::string(text)};
  std::string line;
  while (std::getline(lines, line)) {
    std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
      continue;
    }
    if (startsWith(trimmed, "- ") || startsWith(trimmed, "* ")) {
      trimmed = trimCopy(std::string_view(trimmed).substr(2u));
    } else if (isNumberedRule(trimmed)) {
      const std::size_t dot = trimmed.find('.');
      trimmed = trimCopy(std::string_view(trimmed).substr(dot + 1u));
    } else {
      continue;
    }
    if (trimmed.empty()) {
      continue;
    }
    if (max_rule_chars > 0u && trimmed.size() > max_rule_chars) {
      trimmed = trimmed.substr(0u, max_rule_chars);
      trimmed += "...";
    }
    rules.push_back(std::move(trimmed));
  }
  return rules;
}

[[nodiscard]] std::vector<std::string> tokenizeShellCommand(const std::string_view command) {
  std::vector<std::string> tokens;
  std::string token;
  char quote = '\0';
  bool escaping = false;
  for (const char c : command) {
    if (escaping) {
      token.push_back(c);
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (quote != '\0') {
      if (c == quote) {
        quote = '\0';
      } else {
        token.push_back(c);
      }
      continue;
    }
    if (c == '\'' || c == '"') {
      quote = c;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      if (!token.empty()) {
        tokens.push_back(std::move(token));
        token.clear();
      }
      continue;
    }
    token.push_back(c);
  }
  if (!token.empty()) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

[[nodiscard]] bool commandMatchesPrefix(const std::vector<std::string> &command,
                                        const std::vector<std::string> &prefix) {
  if (prefix.empty() || command.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0u; i < prefix.size(); ++i) {
    if (command[i] != prefix[i]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] int decisionRank(const AsterAgentCommandDecision decision) {
  switch (decision) {
  case AsterAgentCommandDecision::Allow:
    return 1;
  case AsterAgentCommandDecision::Review:
    return 2;
  case AsterAgentCommandDecision::Deny:
    return 3;
  }
  return 2;
}

[[nodiscard]] bool textContainsToken(const std::string_view text, const std::string_view token) {
  return lowerSnake(text).find(lowerSnake(token)) != std::string::npos;
}

[[nodiscard]] bool iterationClaimsSignal(const AsterAgentAssetIteration &iteration,
                                         const std::string_view signal_id) {
  if (containsString(iteration.claimed_signals, signal_id)) {
    return true;
  }
  return textContainsToken(iteration.notes, signal_id);
}

[[nodiscard]] bool iterationRejectsSignal(const AsterAgentAssetIteration &iteration,
                                          const std::string_view signal_id) {
  return containsString(iteration.rejected_signals, signal_id) ||
         textContainsToken(iteration.notes, std::string("reject_") + std::string(signal_id)) ||
         textContainsToken(iteration.notes, std::string("without_") + std::string(signal_id));
}

[[nodiscard]] std::string signalListText(
    const std::vector<AsterAgentAssetQualitySignal> &signals) {
  std::ostringstream out;
  for (const AsterAgentAssetQualitySignal &signal : signals) {
    out << "- " << signal.id << " weight=" << signal.weight << ": " << signal.description << "\n";
  }
  return out.str();
}

[[nodiscard]] AsterAgentValidationCommand makeValidation(std::string id, std::string command,
                                                         std::filesystem::path working_directory,
                                                         std::vector<AsterAgentDomain> domains,
                                                         const bool required = true) {
  return {.id = std::move(id),
          .command = std::move(command),
          .working_directory = std::move(working_directory),
          .domains = std::move(domains),
          .required = required};
}

[[nodiscard]] AsterAgentTask makeTask(std::string id, std::string title, std::string brief,
                                      std::vector<AsterAgentDomain> domains,
                                      std::vector<std::string> depends_on,
                                      std::vector<std::filesystem::path> touched_paths,
                                      const AsterAgentTaskStatus status, const int priority,
                                      const std::uint32_t estimated_steps) {
  return {.id = std::move(id),
          .title = std::move(title),
          .brief = std::move(brief),
          .domains = std::move(domains),
          .depends_on = std::move(depends_on),
          .touched_paths = std::move(touched_paths),
          .status = status,
          .priority = priority,
          .estimated_steps = estimated_steps};
}

} // namespace

bool AsterAgentWorkspaceAudit::ok() const {
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
      return false;
    }
  }
  return true;
}

bool AsterAgentInstructionStack::ok() const {
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> AsterAgentInstructionStack::flattenedRules() const {
  std::vector<std::string> rules;
  for (const AsterAgentInstructionFile &file : files) {
    rules.insert(rules.end(), file.rules.begin(), file.rules.end());
  }
  return rules;
}

std::uint64_t AsterAgentInstructionStack::contractStamp() const {
  std::uint64_t stamp = 0xA57E1A5712570001ull;
  for (const AsterAgentInstructionFile &file : files) {
    stamp = hashCombine(stamp, stableStringStamp(pathText(file.path)));
    stamp = hashCombine(stamp, stableStringStamp(pathText(file.scope_root)));
    stamp = hashCombine(stamp, file.depth);
    for (const std::string &rule : file.rules) {
      stamp = hashCombine(stamp, stableStringStamp(rule));
    }
  }
  return stamp;
}

std::string AsterAgentInstructionStack::summarizeMarkdown() const {
  std::ostringstream out;
  out << "# Aster Agent Instruction Stack\n\n";
  if (files.empty()) {
    out << "No scoped instruction files were found.\n";
  }
  for (const AsterAgentInstructionFile &file : files) {
    out << "## " << pathText(file.path) << "\n";
    out << "scope: " << pathText(file.scope_root) << " depth: " << file.depth << "\n";
    for (const std::string &rule : file.rules) {
      out << "- " << rule << "\n";
    }
    out << "\n";
  }
  if (!diagnostics.empty()) {
    out << "## Diagnostics\n";
    for (const Diagnostic &diagnostic : diagnostics) {
      out << "- " << (diagnostic.severity == DiagnosticSeverity::Error ? "error" : "warning")
          << " " << diagnostic.path << ": " << diagnostic.message << "\n";
    }
  }
  out << "stamp: " << contractStamp() << "\n";
  return out.str();
}

AsterAgentInstructionStack loadAsterAgentInstructions(
    const std::filesystem::path &start_path, AsterAgentInstructionOptions options) {
  AsterAgentInstructionStack stack;
  if (options.file_name.empty()) {
    addDiagnostic(stack.diagnostics, DiagnosticSeverity::Error, start_path, "$.file_name",
                  "instruction file name cannot be empty");
    return stack;
  }

  std::filesystem::path scope = start_path.lexically_normal();
  if (scope.filename() == options.file_name || scope.has_extension()) {
    scope = scope.parent_path();
  }
  if (scope.empty()) {
    scope = ".";
  }

  std::filesystem::path root =
      options.workspace_root.empty() ? scope : options.workspace_root.lexically_normal();
  if (root.is_absolute() && scope.is_relative()) {
    scope = absoluteLexical(scope);
  } else if (root.is_relative() && scope.is_absolute()) {
    root = absoluteLexical(root);
  }
  if (!pathWithinRoot(scope, root)) {
    addDiagnostic(stack.diagnostics, DiagnosticSeverity::Warning, start_path, "$.workspace_root",
                  "workspace root is not an ancestor of the requested path; using local scope");
    root = scope;
  }

  std::vector<std::filesystem::path> chain;
  for (std::filesystem::path current = scope.lexically_normal(); !current.empty();
       current = current.parent_path()) {
    chain.push_back(current);
    if (current == root || current == current.parent_path()) {
      break;
    }
  }
  std::reverse(chain.begin(), chain.end());

  for (std::size_t index = 0u; index < chain.size(); ++index) {
    const std::filesystem::path instruction_path = chain[index] / options.file_name;
    if (!std::filesystem::exists(instruction_path)) {
      continue;
    }
    std::ifstream input(instruction_path);
    if (!input) {
      addDiagnostic(stack.diagnostics, DiagnosticSeverity::Warning, instruction_path, "$.read",
                    "could not read scoped agent instructions");
      continue;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    AsterAgentInstructionFile file;
    file.path = instruction_path.lexically_normal();
    file.scope_root = chain[index].lexically_normal();
    file.depth = static_cast<std::uint32_t>(index);
    file.rules = instructionRulesFromText(buffer.str(), options.max_rule_chars);
    stack.files.push_back(std::move(file));
  }
  return stack;
}

bool AsterAgentCommandPolicy::addRule(AsterAgentCommandRule rule) {
  if (rule.id.empty() || rule.prefix.empty()) {
    return false;
  }
  if (std::any_of(rules_.begin(), rules_.end(),
                  [&](const AsterAgentCommandRule &existing) { return existing.id == rule.id; })) {
    return false;
  }
  rules_.push_back(std::move(rule));
  return true;
}

AsterAgentCommandReview
AsterAgentCommandPolicy::reviewCommand(std::vector<std::string> command) const {
  AsterAgentCommandReview review;
  review.command = std::move(command);
  const AsterAgentCommandRule *best = nullptr;
  for (const AsterAgentCommandRule &rule : rules_) {
    if (!commandMatchesPrefix(review.command, rule.prefix)) {
      continue;
    }
    if (best == nullptr || rule.prefix.size() > best->prefix.size() ||
        (rule.prefix.size() == best->prefix.size() &&
         decisionRank(rule.decision) > decisionRank(best->decision))) {
      best = &rule;
    }
  }
  if (best == nullptr) {
    review.decision = AsterAgentCommandDecision::Review;
    review.rule_id = "aster.default.review_unknown";
    review.rationale = "Command has no Aster-owned allow or deny prefix.";
    return review;
  }
  review.decision = best->decision;
  review.rule_id = best->id;
  review.rationale = best->rationale;
  return review;
}

AsterAgentCommandReview
AsterAgentCommandPolicy::reviewShellCommand(const std::string_view command) const {
  return reviewCommand(tokenizeShellCommand(command));
}

const std::vector<AsterAgentCommandRule> &AsterAgentCommandPolicy::rules() const noexcept {
  return rules_;
}

std::uint64_t AsterAgentCommandPolicy::contractStamp() const {
  std::uint64_t stamp = 0xA57EC0A4D5010001ull;
  for (const AsterAgentCommandRule &rule : rules_) {
    stamp = hashCombine(stamp, stableStringStamp(rule.id));
    stamp = hashCombine(stamp, static_cast<std::uint64_t>(rule.decision));
    stamp = hashCombine(stamp, stableStringStamp(rule.rationale));
    for (const std::string &part : rule.prefix) {
      stamp = hashCombine(stamp, stableStringStamp(part));
    }
  }
  return stamp;
}

std::string AsterAgentCommandPolicy::summarizeMarkdown() const {
  std::ostringstream out;
  out << "# Aster Agent Command Policy\n\n";
  for (const AsterAgentCommandRule &rule : rules_) {
    out << "- [" << asterAgentCommandDecisionName(rule.decision) << "] " << rule.id << ":";
    for (const std::string &part : rule.prefix) {
      out << " " << part;
    }
    if (!rule.rationale.empty()) {
      out << " - " << rule.rationale;
    }
    out << "\n";
  }
  out << "stamp: " << contractStamp() << "\n";
  return out.str();
}

std::uint64_t AsterAgentRunbook::contractStamp() const {
  std::uint64_t stamp = 0xA57E2A7B001C0001ull;
  stamp = hashCombine(stamp, stableStringStamp(profile.name));
  stamp = hashCombine(stamp, stableStringStamp(profile.objective));
  stamp = hashCombine(stamp, stableStringStamp(pathText(profile.project_root)));
  stamp = hashCombine(stamp, instructions.contractStamp());
  stamp = hashCombine(stamp, command_policy.contractStamp());
  for (const std::string &note : notes) {
    stamp = hashCombine(stamp, stableStringStamp(note));
  }
  return stamp;
}

std::string AsterAgentRunbook::summarizeMarkdown() const {
  std::ostringstream out;
  out << "# Aster Agent Runbook\n\n";
  out << "workspace: " << profile.name << "\n";
  out << "objective: " << profile.objective << "\n";
  out << "project: " << pathText(profile.project_file) << "\n\n";
  if (!notes.empty()) {
    out << "## Notes\n";
    for (const std::string &note : notes) {
      out << "- " << note << "\n";
    }
    out << "\n";
  }
  out << instructions.summarizeMarkdown() << "\n";
  out << command_policy.summarizeMarkdown() << "\n";
  out << "runbook_stamp: " << contractStamp() << "\n";
  return out.str();
}

bool AsterAgentTaskBoard::addTask(AsterAgentTask task) {
  if (task.id.empty() || this->task(task.id) != nullptr) {
    return false;
  }
  if (task.title.empty()) {
    task.title = task.id;
  }
  tasks_.push_back(std::move(task));
  return true;
}

bool AsterAgentTaskBoard::addBatch(AsterAgentBatch batch) {
  if (batch.id.empty()) {
    return false;
  }
  if (std::any_of(batches_.begin(), batches_.end(),
                  [&](const AsterAgentBatch &existing) { return existing.id == batch.id; })) {
    return false;
  }
  for (const AsterAgentTask &task_entry : batch.tasks) {
    if (task_entry.id.empty() || task(task_entry.id) != nullptr) {
      return false;
    }
  }
  for (const AsterAgentTask &task_entry : batch.tasks) {
    tasks_.push_back(task_entry);
  }
  batches_.push_back(std::move(batch));
  return true;
}

bool AsterAgentTaskBoard::setStatus(const std::string_view id,
                                    const AsterAgentTaskStatus status) {
  bool changed = false;
  for (AsterAgentTask &task_entry : tasks_) {
    if (task_entry.id == id) {
      task_entry.status = status;
      changed = true;
    }
  }
  for (AsterAgentBatch &batch : batches_) {
    for (AsterAgentTask &task_entry : batch.tasks) {
      if (task_entry.id == id) {
        task_entry.status = status;
      }
    }
  }
  return changed;
}

const AsterAgentTask *AsterAgentTaskBoard::task(const std::string_view id) const {
  const auto found = std::find_if(tasks_.begin(), tasks_.end(), [&](const AsterAgentTask &task_entry) {
    return task_entry.id == id;
  });
  return found == tasks_.end() ? nullptr : &*found;
}

std::vector<AsterAgentTask> AsterAgentTaskBoard::tasks() const {
  return tasks_;
}

const std::vector<AsterAgentBatch> &AsterAgentTaskBoard::batches() const noexcept {
  return batches_;
}

std::vector<AsterAgentTask> AsterAgentTaskBoard::readyTasks() const {
  std::vector<AsterAgentTask> ready;
  for (const AsterAgentTask &task_entry : tasks_) {
    if ((task_entry.status == AsterAgentTaskStatus::Planned ||
         task_entry.status == AsterAgentTaskStatus::Ready) &&
        dependenciesReady(task_entry)) {
      ready.push_back(task_entry);
    }
  }
  std::sort(ready.begin(), ready.end(), [](const AsterAgentTask &lhs, const AsterAgentTask &rhs) {
    if (lhs.priority != rhs.priority) {
      return lhs.priority > rhs.priority;
    }
    return lhs.id < rhs.id;
  });
  return ready;
}

std::vector<AsterAgentTask> AsterAgentTaskBoard::blockedTasks() const {
  std::vector<AsterAgentTask> blocked;
  for (const AsterAgentTask &task_entry : tasks_) {
    if (task_entry.status == AsterAgentTaskStatus::Blocked || hasMissingDependency(task_entry) ||
        (!dependenciesReady(task_entry) && task_entry.status != AsterAgentTaskStatus::Complete)) {
      blocked.push_back(task_entry);
    }
  }
  return blocked;
}

std::uint64_t AsterAgentTaskBoard::contractStamp() const {
  std::uint64_t stamp = 0xA57EACE17A510001ull;
  for (const AsterAgentTask &task_entry : tasks_) {
    stamp = hashCombine(stamp, stableStringStamp(task_entry.id));
    stamp = hashCombine(stamp, stableStringStamp(task_entry.title));
    stamp = hashCombine(stamp, static_cast<std::uint64_t>(task_entry.status));
    stamp = hashCombine(stamp, static_cast<std::uint64_t>(task_entry.priority));
    for (const std::string &dependency : task_entry.depends_on) {
      stamp = hashCombine(stamp, stableStringStamp(dependency));
    }
    for (const std::filesystem::path &path : task_entry.touched_paths) {
      stamp = hashCombine(stamp, stableStringStamp(pathText(path)));
    }
  }
  for (const AsterAgentBatch &batch : batches_) {
    stamp = hashCombine(stamp, stableStringStamp(batch.id));
    stamp = hashCombine(stamp, static_cast<std::uint64_t>(batch.policy));
  }
  return stamp;
}

std::string AsterAgentTaskBoard::summarizeMarkdown() const {
  std::ostringstream out;
  out << "# Aster Agent Task Board\n\n";
  for (const AsterAgentBatch &batch : batches_) {
    out << "## " << batch.id << " - " << batch.title << "\n";
    out << "policy: " << asterAgentBatchPolicyName(batch.policy) << "\n";
    for (const AsterAgentTask &task_entry : batch.tasks) {
      out << "- [" << asterAgentTaskStatusName(task_entry.status) << "] " << task_entry.id
          << ": " << task_entry.title;
      if (!task_entry.depends_on.empty()) {
        out << " after ";
        for (std::size_t i = 0u; i < task_entry.depends_on.size(); ++i) {
          if (i > 0u) {
            out << ", ";
          }
          out << task_entry.depends_on[i];
        }
      }
      out << "\n";
    }
    out << "\n";
  }
  out << "stamp: " << contractStamp() << "\n";
  return out.str();
}

bool AsterAgentTaskBoard::dependencyComplete(const std::string_view id) const {
  const AsterAgentTask *dependency = task(id);
  return dependency != nullptr && dependency->status == AsterAgentTaskStatus::Complete;
}

bool AsterAgentTaskBoard::dependenciesReady(const AsterAgentTask &task_entry) const {
  return std::all_of(task_entry.depends_on.begin(), task_entry.depends_on.end(),
                     [&](const std::string &dependency) { return dependencyComplete(dependency); });
}

bool AsterAgentTaskBoard::hasMissingDependency(const AsterAgentTask &task_entry) const {
  return std::any_of(task_entry.depends_on.begin(), task_entry.depends_on.end(),
                     [&](const std::string &dependency) { return task(dependency) == nullptr; });
}

std::string_view asterAgentDomainName(const AsterAgentDomain domain) {
  switch (domain) {
  case AsterAgentDomain::Unknown:
    return "unknown";
  case AsterAgentDomain::Project:
    return "project";
  case AsterAgentDomain::Scene:
    return "scene";
  case AsterAgentDomain::Prefab:
    return "prefab";
  case AsterAgentDomain::Material:
    return "material";
  case AsterAgentDomain::Item:
    return "item";
  case AsterAgentDomain::ActionGraph:
    return "action_graph";
  case AsterAgentDomain::Geometry:
    return "geometry";
  case AsterAgentDomain::Physics:
    return "physics";
  case AsterAgentDomain::Systems:
    return "systems";
  case AsterAgentDomain::Rendering:
    return "rendering";
  case AsterAgentDomain::Ui:
    return "ui";
  case AsterAgentDomain::Build:
    return "build";
  case AsterAgentDomain::Tests:
    return "tests";
  }
  return "unknown";
}

AsterAgentDomain parseAsterAgentDomain(const std::string_view value) {
  const std::string normalized = lowerSnake(value);
  if (normalized == "project") {
    return AsterAgentDomain::Project;
  }
  if (normalized == "scene") {
    return AsterAgentDomain::Scene;
  }
  if (normalized == "prefab") {
    return AsterAgentDomain::Prefab;
  }
  if (normalized == "material") {
    return AsterAgentDomain::Material;
  }
  if (normalized == "item") {
    return AsterAgentDomain::Item;
  }
  if (normalized == "action_graph" || normalized == "actiongraph") {
    return AsterAgentDomain::ActionGraph;
  }
  if (normalized == "geometry") {
    return AsterAgentDomain::Geometry;
  }
  if (normalized == "physics") {
    return AsterAgentDomain::Physics;
  }
  if (normalized == "systems") {
    return AsterAgentDomain::Systems;
  }
  if (normalized == "rendering" || normalized == "render") {
    return AsterAgentDomain::Rendering;
  }
  if (normalized == "ui") {
    return AsterAgentDomain::Ui;
  }
  if (normalized == "build") {
    return AsterAgentDomain::Build;
  }
  if (normalized == "tests" || normalized == "test") {
    return AsterAgentDomain::Tests;
  }
  return AsterAgentDomain::Unknown;
}

std::string_view asterAgentTaskStatusName(const AsterAgentTaskStatus status) {
  switch (status) {
  case AsterAgentTaskStatus::Planned:
    return "planned";
  case AsterAgentTaskStatus::Ready:
    return "ready";
  case AsterAgentTaskStatus::InProgress:
    return "in_progress";
  case AsterAgentTaskStatus::Blocked:
    return "blocked";
  case AsterAgentTaskStatus::Complete:
    return "complete";
  }
  return "planned";
}

std::string_view asterAgentBatchPolicyName(const AsterAgentBatchPolicy policy) {
  switch (policy) {
  case AsterAgentBatchPolicy::Serial:
    return "serial";
  case AsterAgentBatchPolicy::ParallelSafe:
    return "parallel_safe";
  case AsterAgentBatchPolicy::ReviewGate:
    return "review_gate";
  }
  return "serial";
}

std::string_view asterAgentAssetReferenceRoleName(const AsterAgentAssetReferenceRole role) {
  switch (role) {
  case AsterAgentAssetReferenceRole::VisualTarget:
    return "visual_target";
  case AsterAgentAssetReferenceRole::ShapeTarget:
    return "shape_target";
  case AsterAgentAssetReferenceRole::MaterialTarget:
    return "material_target";
  case AsterAgentAssetReferenceRole::NegativeExample:
    return "negative_example";
  case AsterAgentAssetReferenceRole::CandidateOutput:
    return "candidate_output";
  }
  return "visual_target";
}

std::string_view asterAgentAssetReviewStatusName(const AsterAgentAssetReviewStatus status) {
  switch (status) {
  case AsterAgentAssetReviewStatus::NeedsWork:
    return "needs_work";
  case AsterAgentAssetReviewStatus::Passed:
    return "passed";
  case AsterAgentAssetReviewStatus::Blocked:
    return "blocked";
  }
  return "needs_work";
}

std::string_view asterAgentCommandDecisionName(const AsterAgentCommandDecision decision) {
  switch (decision) {
  case AsterAgentCommandDecision::Allow:
    return "allow";
  case AsterAgentCommandDecision::Review:
    return "review";
  case AsterAgentCommandDecision::Deny:
    return "deny";
  }
  return "review";
}

std::vector<AsterAgentDomain> asterAgentDomainsForAssetKind(const AssetKind kind) {
  switch (kind) {
  case AssetKind::Scene:
    return {AsterAgentDomain::Project, AsterAgentDomain::Scene};
  case AssetKind::Prefab:
    return {AsterAgentDomain::Prefab, AsterAgentDomain::Systems};
  case AssetKind::Cave:
  case AssetKind::Mesh:
  case AssetKind::AssetGraph:
    return {AsterAgentDomain::Geometry, AsterAgentDomain::Scene};
  case AssetKind::Material:
  case AssetKind::Texture:
    return {AsterAgentDomain::Material, AsterAgentDomain::Rendering};
  case AssetKind::Item:
    return {AsterAgentDomain::Item, AsterAgentDomain::Systems};
  case AssetKind::ActionGraph:
    return {AsterAgentDomain::ActionGraph, AsterAgentDomain::Systems};
  case AssetKind::InputMap:
    return {AsterAgentDomain::Systems, AsterAgentDomain::Ui};
  case AssetKind::Ui:
    return {AsterAgentDomain::Ui};
  case AssetKind::Unknown:
    return {AsterAgentDomain::Unknown};
  }
  return {AsterAgentDomain::Unknown};
}

AsterAgentWorkspaceProfile
createAsterAgentWorkspaceProfile(const ProjectDocument &project,
                                 const std::filesystem::path &project_root,
                                 AsterAgentWorkspaceOptions options) {
  AsterAgentWorkspaceProfile profile;
  profile.name =
      project.name.empty() ? "Aster Agent Workspace" : project.name + " Agent Workspace";
  profile.objective = options.objective.empty()
                          ? "Grow an Aster game project through schema-first content batches."
                          : std::move(options.objective);
  profile.project_root = project_root.lexically_normal();
  profile.project_file = options.project_file.empty() ? profile.project_root / "project.asterproj"
                                                       : options.project_file.lexically_normal();
  profile.additional_roots = std::move(options.additional_roots);
  profile.metadata["asset_count"] = std::to_string(project.assets.size());
  profile.metadata["startup_scene"] = project.startup_scene;
  profile.metadata["kernel_changes"] = options.allow_kernel_changes ? "allowed" : "locked";

  std::map<std::string, AsterAgentScopeRule> scope_by_path;
  auto ensure_scope = [&](std::filesystem::path path, std::string owner,
                          std::vector<AsterAgentDomain> domains, const bool writable) {
    path = path.lexically_normal();
    const std::string key = pathText(path);
    AsterAgentScopeRule &scope = scope_by_path[key];
    if (scope.path.empty()) {
      scope.path = std::move(path);
      scope.owner = std::move(owner);
      scope.writable = writable;
    }
    scope.writable = scope.writable && writable;
    mergeDomains(scope.domains, domains);
  };

  ensure_scope(profile.project_file, "project-manifest", {AsterAgentDomain::Project}, true);
  for (const ProjectAssetRef &asset : project.assets) {
    const std::filesystem::path parent =
        asset.path.parent_path().empty() ? std::filesystem::path(".") : asset.path.parent_path();
    ensure_scope(profile.project_root / parent, std::string(assetKindName(asset.kind)),
                 asterAgentDomainsForAssetKind(asset.kind), true);
  }
  if (!options.allow_kernel_changes) {
    ensure_scope("include/aster/kernel", "kernel-abi", {AsterAgentDomain::Build}, false);
  }
  for (const auto &[unused, scope] : scope_by_path) {
    (void)unused;
    profile.scopes.push_back(scope);
  }

  if (options.include_default_validation) {
    profile.validation.push_back(makeValidation(
        "build.game_sdk_public_consumer",
        "cmake --build <build-dir> --target aster_game_sdk_public_consumer", {},
        {AsterAgentDomain::Build, AsterAgentDomain::Tests}));
    profile.validation.push_back(makeValidation(
        "test.game_sdk_public_consumer",
        "ctest --test-dir <build-dir> --output-on-failure -R aster_game_sdk_public_consumer", {},
        {AsterAgentDomain::Tests}));
    profile.validation.push_back(makeValidation(
        "cook.project",
        "cargo run -p aster_assetc --bin aster_assetc -- cook --project " +
            shellQuotePath(profile.project_file) + " --platform desktop --output <cook-output-dir>",
        profile.project_root, {AsterAgentDomain::Build, AsterAgentDomain::Project}, false));
  }

  profile.output_contracts.push_back(
      {.id = "agent_batch_report",
       .description = "Structured result for one Aster authoring batch.",
       .required_keys = {"schema_version", "batch_id", "summary", "changed_files", "validation",
                         "handoff"}});
  profile.output_contracts.push_back(
      {.id = "aster_handoff",
       .description = "Compact continuation record for the next agent turn.",
       .required_keys = {"session_id", "decisions", "remaining_tasks", "risk_notes"}});
  return profile;
}

AsterAgentWorkspaceAudit auditAsterAgentWorkspace(const ProjectDocument &project,
                                                  const AsterAgentWorkspaceProfile &profile) {
  AsterAgentWorkspaceAudit audit;
  const std::filesystem::path source = profile.project_file;
  if (project.schema_version != kAuthoringSchemaVersion) {
    addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source, "$.schema_version",
                  "project is not using the active Aster authoring schema");
  }
  if (project.name.empty()) {
    addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source, "$.name",
                  "project name is required for agent handoffs");
  }
  if (project.startup_scene.empty()) {
    addDiagnostic(audit.diagnostics, DiagnosticSeverity::Warning, source, "$.startup_scene",
                  "startup scene is empty; agents cannot infer the first playable loop");
  }
  if (profile.validation.empty()) {
    addDiagnostic(audit.diagnostics, DiagnosticSeverity::Warning, source, "$.agent.validation",
                  "agent profile has no validation commands");
  }

  std::set<std::string> asset_ids;
  bool startup_found = project.startup_scene.empty();
  std::set<std::string> recommended;
  for (const ProjectAssetRef &asset : project.assets) {
    AsterAgentAssetSignal signal;
    signal.id = asset.id;
    signal.kind = asset.kind;
    signal.path = asset.path;
    signal.domains = asterAgentDomainsForAssetKind(asset.kind);
    signal.startup = asset.id == project.startup_scene;
    audit.assets.push_back(signal);

    if (asset.id.empty()) {
      addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source, "$.assets",
                    "asset id is required for agent-safe ownership");
    } else if (!asset_ids.insert(asset.id).second) {
      addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source, "$.assets." + asset.id,
                    "duplicate asset id");
    }
    if (asset.kind == AssetKind::Unknown) {
      addDiagnostic(audit.diagnostics, DiagnosticSeverity::Warning, source,
                    "$.assets." + asset.id + ".kind", "asset kind is unknown");
    }
    if (asset.path.empty()) {
      addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source,
                    "$.assets." + asset.id + ".path", "asset path is required");
    }
    if (pathEscapesRoot(asset.path)) {
      addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source,
                    "$.assets." + asset.id + ".path", "asset path escapes the project root");
    }
    if (signal.startup) {
      startup_found = true;
    }
    for (const AsterAgentDomain domain : signal.domains) {
      if (domain != AsterAgentDomain::Unknown) {
        recommended.insert(std::string(asterAgentDomainName(domain)));
      }
    }
  }
  if (!startup_found) {
    addDiagnostic(audit.diagnostics, DiagnosticSeverity::Error, source, "$.startup_scene",
                  "startup scene does not match a project asset id");
  }
  audit.recommended_batches.assign(recommended.begin(), recommended.end());
  std::sort(audit.assets.begin(), audit.assets.end(),
            [](const AsterAgentAssetSignal &lhs, const AsterAgentAssetSignal &rhs) {
              return lhs.id < rhs.id;
            });
  return audit;
}

AsterAgentTaskBoard planAsterAgentAuthoringBatches(
    const ProjectDocument &project, const AsterAgentWorkspaceProfile &profile) {
  AsterAgentTaskBoard board;
  const std::vector<std::filesystem::path> project_paths = {profile.project_file};

  AsterAgentBatch foundation;
  foundation.id = "batch.foundation";
  foundation.title = "Map project contracts";
  foundation.policy = AsterAgentBatchPolicy::ReviewGate;
  foundation.tasks.push_back(makeTask(
      "agent.map_project_contracts", "Map project contracts",
      "Read the Aster project manifest, startup scene, and asset ownership before editing.",
      {AsterAgentDomain::Project, AsterAgentDomain::Build}, {}, project_paths,
      AsterAgentTaskStatus::Ready, 100, 2u));
  foundation.tasks.push_back(makeTask(
      "agent.normalize_authoring_surface", "Normalize authoring surface",
      "Group scene, prefab, material, item, and action graph files into stable Aster scopes.",
      {AsterAgentDomain::Project, AsterAgentDomain::Scene}, {"agent.map_project_contracts"},
      scopePathsForDomain(profile, AsterAgentDomain::Project), AsterAgentTaskStatus::Planned, 90,
      2u));
  (void)board.addBatch(std::move(foundation));

  AsterAgentBatch content;
  content.id = "batch.content_surface";
  content.title = "Build game content surface";
  content.policy = AsterAgentBatchPolicy::ParallelSafe;
  if (hasAssetKind(project, {AssetKind::Scene, AssetKind::Cave, AssetKind::Mesh})) {
    content.tasks.push_back(makeTask(
        "agent.scene_foundation", "Scene and traversal foundation",
        "Make scene, cave, mesh, and placement documents understandable to future agents.",
        {AsterAgentDomain::Scene, AsterAgentDomain::Geometry},
        {"agent.normalize_authoring_surface"},
        assetPathsFor(project, {AssetKind::Scene, AssetKind::Cave, AssetKind::Mesh}),
        AsterAgentTaskStatus::Planned, 80, 3u));
  }
  if (hasAssetKind(project, {AssetKind::Prefab})) {
    content.tasks.push_back(makeTask(
        "agent.prefab_contracts", "Prefab interaction contracts",
        "Keep prefab roots, sockets, and component ownership explicit enough for batch edits.",
        {AsterAgentDomain::Prefab, AsterAgentDomain::Systems},
        {"agent.normalize_authoring_surface"}, assetPathsFor(project, {AssetKind::Prefab}),
        AsterAgentTaskStatus::Planned, 70, 2u));
  }
  if (hasAssetKind(project, {AssetKind::Material, AssetKind::Texture, AssetKind::AssetGraph})) {
    content.tasks.push_back(makeTask(
        "agent.visual_proof_surface", "Visual proof surface",
        "Connect material, texture, and procedural graph assets to renderer-facing proof commands.",
        {AsterAgentDomain::Material, AsterAgentDomain::Rendering},
        {"agent.normalize_authoring_surface"},
        assetPathsFor(project, {AssetKind::Material, AssetKind::Texture, AssetKind::AssetGraph}),
        AsterAgentTaskStatus::Planned, 70, 3u));
  }
  if (!content.tasks.empty()) {
    (void)board.addBatch(std::move(content));
  }

  AsterAgentBatch systems;
  systems.id = "batch.gameplay_systems";
  systems.title = "Bind gameplay systems";
  systems.policy = AsterAgentBatchPolicy::Serial;
  if (hasAssetKind(project, {AssetKind::Item, AssetKind::ActionGraph, AssetKind::InputMap,
                             AssetKind::Ui})) {
    std::vector<std::string> gameplay_dependencies;
    if (board.task("agent.scene_foundation") != nullptr) {
      gameplay_dependencies.push_back("agent.scene_foundation");
    } else {
      gameplay_dependencies.push_back("agent.normalize_authoring_surface");
    }
    systems.tasks.push_back(makeTask(
        "agent.gameplay_loop_contracts", "Gameplay loop contracts",
        "Tie items, action graphs, input maps, and UI files to entity/component documents.",
        {AsterAgentDomain::Item, AsterAgentDomain::ActionGraph, AsterAgentDomain::Systems,
         AsterAgentDomain::Ui},
        std::move(gameplay_dependencies),
        assetPathsFor(project, {AssetKind::Item, AssetKind::ActionGraph, AssetKind::InputMap,
                                AssetKind::Ui}),
        AsterAgentTaskStatus::Planned, 60, 3u));
  }
  if (!systems.tasks.empty()) {
    (void)board.addBatch(std::move(systems));
  }

  AsterAgentBatch proof;
  proof.id = "batch.proof";
  proof.title = "Cook, test, and hand off";
  proof.policy = AsterAgentBatchPolicy::ReviewGate;
  proof.validation = profile.validation;
  std::vector<std::string> proof_deps = {"agent.normalize_authoring_surface"};
  if (board.task("agent.scene_foundation") != nullptr) {
    proof_deps.push_back("agent.scene_foundation");
  }
  if (board.task("agent.prefab_contracts") != nullptr) {
    proof_deps.push_back("agent.prefab_contracts");
  }
  if (board.task("agent.visual_proof_surface") != nullptr) {
    proof_deps.push_back("agent.visual_proof_surface");
  }
  if (board.task("agent.gameplay_loop_contracts") != nullptr) {
    proof_deps.push_back("agent.gameplay_loop_contracts");
  }
  proof.tasks.push_back(makeTask(
      "agent.validation_handoff", "Validation and handoff",
      "Run the smallest useful Aster validation set and leave a continuation-safe report.",
      {AsterAgentDomain::Build, AsterAgentDomain::Tests}, std::move(proof_deps), {},
      AsterAgentTaskStatus::Planned, 50, 2u));
  (void)board.addBatch(std::move(proof));
  return board;
}

AsterAgentCommandPolicy createDefaultAsterAgentCommandPolicy(const bool allow_kernel_changes) {
  AsterAgentCommandPolicy policy;
  const auto add = [&](std::string id, std::vector<std::string> prefix,
                       const AsterAgentCommandDecision decision, std::string rationale) {
    (void)policy.addRule({.id = std::move(id),
                          .prefix = std::move(prefix),
                          .decision = decision,
                          .rationale = std::move(rationale)});
  };

  add("aster.allow.git_status", {"git", "status"}, AsterAgentCommandDecision::Allow,
      "Read-only git state is part of every safe Aster batch.");
  add("aster.allow.git_diff", {"git", "diff"}, AsterAgentCommandDecision::Allow,
      "Diff inspection is required to avoid stepping on another agent's work.");
  add("aster.allow.search", {"rg"}, AsterAgentCommandDecision::Allow,
      "Fast repository search is safe and expected.");
  add("aster.allow.read", {"sed"}, AsterAgentCommandDecision::Allow,
      "Bounded file reads are safe for context gathering.");
  add("aster.allow.cmake_build", {"cmake", "--build"}, AsterAgentCommandDecision::Allow,
      "Aster validation uses targeted CMake build proof.");
  add("aster.allow.ctest", {"ctest"}, AsterAgentCommandDecision::Allow,
      "Targeted CTest validation is part of the public contract proof.");
  add("aster.allow.assetc_agent_plan", {"cargo", "run", "-p", "aster_assetc"},
      AsterAgentCommandDecision::Allow,
      "The asset compiler owns machine-readable Aster authoring reports.");
  add("aster.allow.cargo_test_assetc", {"cargo", "test", "-p", "aster_assetc"},
      AsterAgentCommandDecision::Allow,
      "Asset compiler tests are the narrow proof for agent-plan changes.");
  add("aster.review.git_stage", {"git", "add"}, AsterAgentCommandDecision::Review,
      "Staging should happen only as an explicit handoff action.");
  add("aster.review.git_commit", {"git", "commit"}, AsterAgentCommandDecision::Review,
      "Commits are durable collaboration actions and need an explicit user request.");
  add("aster.deny.git_reset_hard", {"git", "reset", "--hard"}, AsterAgentCommandDecision::Deny,
      "Destructive reset can erase another agent's or user's work.");
  add("aster.deny.git_checkout_paths", {"git", "checkout", "--"}, AsterAgentCommandDecision::Deny,
      "Path checkout can silently revert unrelated local edits.");
  add("aster.deny.remove_tree", {"rm", "-rf"}, AsterAgentCommandDecision::Deny,
      "Recursive deletion is outside normal Aster authoring batches.");
  add("aster.deny.publish_crate", {"cargo", "publish"}, AsterAgentCommandDecision::Deny,
      "Publishing artifacts is not part of local engine iteration.");
  add("aster.deny.third_party_notice", {"touch", "THIRD_PARTY_NOTICES"},
      AsterAgentCommandDecision::Deny,
      "Aster-owned batches must not add third-party notice files.");
  if (allow_kernel_changes) {
    add("aster.review.kernel_contract", {"include/aster/kernel"}, AsterAgentCommandDecision::Review,
        "Kernel ABI edits are allowed only through a contract-update task.");
  } else {
    add("aster.deny.kernel_contract", {"include/aster/kernel"}, AsterAgentCommandDecision::Deny,
        "Kernel ABI is locked for ordinary agent batches.");
  }
  return policy;
}

AsterAgentRunbook createAsterAgentRunbook(const ProjectDocument &project,
                                          const std::filesystem::path &project_root,
                                          AsterAgentRunbookOptions runbook_options,
                                          AsterAgentWorkspaceOptions workspace_options) {
  workspace_options.allow_kernel_changes = workspace_options.allow_kernel_changes ||
                                           runbook_options.allow_kernel_changes;
  AsterAgentRunbook runbook;
  runbook.profile =
      createAsterAgentWorkspaceProfile(project, project_root, std::move(workspace_options));
  if (runbook_options.include_instruction_files) {
    AsterAgentInstructionOptions instruction_options;
    instruction_options.workspace_root =
        runbook_options.workspace_root.empty() ? inferWorkspaceRoot(project_root)
                                               : runbook_options.workspace_root;
    runbook.instructions =
        loadAsterAgentInstructions(runbook.profile.project_file, std::move(instruction_options));
  }
  if (runbook_options.include_default_command_policy) {
    runbook.command_policy =
        createDefaultAsterAgentCommandPolicy(runbook_options.allow_kernel_changes);
  }
  runbook.notes.push_back("Keep public kernel ABI, Game SDK, engine internals, samples, and asset "
                          "compiler ownership boundaries explicit.");
  runbook.notes.push_back("Prefer schema/project documents before adding sample-specific runtime "
                          "shortcuts.");
  runbook.notes.push_back("Leave changed paths, validation, decisions, and remaining tasks in the "
                          "handoff.");
  return runbook;
}

std::string asterAgentBatchOutputSchemaJson() {
  return R"json({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "Aster Agent Batch Report",
  "type": "object",
  "required": ["schema_version", "batch_id", "summary", "changed_files", "validation", "handoff"],
  "properties": {
    "schema_version": { "const": 1 },
    "batch_id": { "type": "string", "minLength": 1 },
    "summary": { "type": "string", "minLength": 1 },
    "changed_files": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "validation": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["command", "status"],
        "properties": {
          "command": { "type": "string" },
          "status": { "enum": ["passed", "failed", "skipped"] },
          "notes": { "type": "string" }
        }
      }
    },
    "handoff": {
      "type": "object",
      "required": ["session_id", "decisions", "remaining_tasks", "risk_notes"],
      "properties": {
        "session_id": { "type": "string" },
        "decisions": { "type": "array", "items": { "type": "string" } },
        "remaining_tasks": { "type": "array", "items": { "type": "string" } },
        "risk_notes": { "type": "array", "items": { "type": "string" } }
      }
    }
  }
})json";
}

std::string asterAgentAssetOutputSchemaJson() {
  return R"json({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "Aster Agent Asset Iteration Report",
  "type": "object",
  "required": [
    "schema_version",
    "asset_id",
    "reference_images_used",
    "changed_files",
    "candidate_artifacts",
    "claimed_signals",
    "rejected_signals",
    "self_review"
  ],
  "properties": {
    "schema_version": { "const": 1 },
    "asset_id": { "type": "string", "minLength": 1 },
    "reference_images_used": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "changed_files": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "candidate_artifacts": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "claimed_signals": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "rejected_signals": {
      "type": "array",
      "items": { "type": "string", "minLength": 1 }
    },
    "self_review": {
      "type": "object",
      "required": ["status", "score", "missing_required", "present_forbidden", "next_actions"],
      "properties": {
        "status": { "enum": ["passed", "needs_work", "blocked"] },
        "score": { "type": "number", "minimum": 0, "maximum": 1 },
        "missing_required": {
          "type": "array",
          "items": { "type": "string" }
        },
        "present_forbidden": {
          "type": "array",
          "items": { "type": "string" }
        },
        "next_actions": {
          "type": "array",
          "items": { "type": "string" }
        }
      }
    }
  }
})json";
}

std::string makeAsterAgentPrompt(const AsterAgentWorkspaceProfile &profile,
                                 const ProjectDocument &project,
                                 const AsterAgentTaskBoard &board) {
  std::ostringstream out;
  out << "You are extending an Aster-owned game project.\n";
  out << "Project: " << project.name << "\n";
  out << "Objective: " << profile.objective << "\n";
  out << "Project root: " << pathText(profile.project_root) << "\n";
  out << "Project file: " << pathText(profile.project_file) << "\n";
  out << "Startup scene: " << project.startup_scene << "\n\n";
  out << "Rules:\n";
  out << "- Keep engine ownership in Aster names, schemas, and modules.\n";
  out << "- Prefer schema-first content edits over sample-specific C++ shortcuts.\n";
  out << "- Do not add third-party notice files as part of an Aster agent batch.\n";
  if (profile.metadata.find("kernel_changes") != profile.metadata.end() &&
      profile.metadata.at("kernel_changes") == "locked") {
    out << "- Treat the kernel ABI as locked unless a task explicitly opens it.\n";
  }
  out << "\nWritable scopes:\n";
  for (const AsterAgentScopeRule &scope : profile.scopes) {
    out << "- " << pathText(scope.path) << " owner=" << scope.owner
        << " writable=" << (scope.writable ? "true" : "false")
        << " domains=" << domainListText(scope.domains) << "\n";
  }
  out << "\nValidation commands:\n";
  for (const AsterAgentValidationCommand &command : profile.validation) {
    out << "- " << command.id << ": " << command.command;
    if (!command.working_directory.empty()) {
      out << " (cwd " << pathText(command.working_directory) << ")";
    }
    if (!command.required) {
      out << " optional";
    }
    out << "\n";
  }
  out << "\nOutput contracts:\n";
  for (const AsterAgentOutputContract &contract : profile.output_contracts) {
    out << "- " << outputContractText(contract) << "\n";
  }
  out << "\n" << board.summarizeMarkdown() << "\n";
  out << "Use this JSON shape for a batch report:\n";
  out << asterAgentBatchOutputSchemaJson() << "\n";
  out << "Profile stamp: " << board.contractStamp() << "\n";
  out << "Project metadata: {\"asset_count\":\"" << project.assets.size()
      << "\",\"startup_scene\":\"" << escapeJson(project.startup_scene) << "\"}\n";
  return out.str();
}

AsterAgentAssetBrief makeIndustrialPipeAssetBrief(std::filesystem::path reference_image,
                                                  AssetId target_asset) {
  AsterAgentAssetBrief brief;
  brief.id = "asset_brief.industrial_pipe_corrosion";
  brief.target_asset = std::move(target_asset);
  brief.title = "Reference-matched rusted industrial pipe";
  brief.target_description =
      "Produce a hollow industrial pipe section that keeps the reference silhouette, visible weld "
      "rings, orange-brown corrosion, dark oxide cavities, pitted metal, and worn rims.";
  brief.references.push_back({.path = std::move(reference_image),
                              .role = AsterAgentAssetReferenceRole::VisualTarget,
                              .note =
                                  "Primary visual target. Preserve rust, welded rings, hollow rim, "
                                  "and industrial scale before inventing details.",
                              .weight = 1.0f});
  brief.required_signals = {
      {"corroded_orange_brown_rust",
       "Orange-brown rust is the dominant material signal, not a clean gray/black pipe.", 1.0f},
      {"dark_oxide_cavities",
       "Dark oxide and grime collect around cavities, inner rim, underside, and weld shadows.",
       0.85f},
      {"raised_weld_rings",
       "Circumferential raised weld bands remain visible and integrated into the pipe body.", 1.0f},
      {"weld_contact_skirts",
       "Weld rings physically touch the pipe through soft contact skirts, grime, and heat tint.",
       0.95f},
      {"open_hollow_rims",
       "Pipe ends read as hollow, thick, dark inner metal with chipped or worn rim edges.", 0.95f},
      {"soft_beveled_rims",
       "Mouth rims avoid knife-edge pixel transitions and use rounded normals or beveling.",
       0.85f},
      {"uneven_pitting",
       "Surface has irregular pitting and blotchy oxidation instead of smooth uniform noise.",
       0.85f},
      {"layered_corrosion_stack",
       "Rust includes oxide, pitting, grime, wet film, exposed metal, and cavity accumulation.",
       0.95f},
      {"physical_roughness_metalness_split",
       "Rough rust, dark grime, wet film, and exposed metal have distinct light response.",
       0.80f},
      {"axial_scratches",
       "Long scratches and wear follow the pipe axis without flattening the material.", 0.65f},
      {"z_fight_free_surface_attachments",
       "Seams, welds, and rim details use inset, clearance, or depth bias instead of coplanar overlap.",
       1.0f},
      {"reference_silhouette",
       "The output keeps the simple reference pipe silhouette unless the brief asks for variants.",
       1.0f}};
  brief.forbidden_signals = {
      {"smooth_black_pipe", "A mostly smooth black or charcoal pipe misses the reference.", 1.0f},
      {"decorative_bolts_without_reference",
       "Bolts, clamp ears, or extra flange hardware must not appear unless explicitly requested.",
       0.9f},
      {"clean_plastic_surface", "Avoid plastic-like smoothness or toy material response.", 0.85f},
      {"monochrome_material", "Avoid one-color rust/noise with no oxide, wear, or depth variation.",
       0.8f},
      {"missing_weld_rings", "Do not remove or hide the reference weld bands.", 1.0f},
      {"floating_weld_rings", "Weld bands must not hover above the pipe or leave a visible gap.",
       1.0f},
      {"coplanar_seam_stripe",
       "Do not leave long z-fighting seam/decal stripes on the pipe body.", 1.0f},
      {"knife_edge_rims", "Pipe mouths must not render as hard black razor edges.", 0.9f},
      {"single_layer_orange_oxide",
       "Avoid a simple orange overlay without grime, pits, weld slag, or metal exposure.", 0.85f}};
  brief.expected_artifacts = {"assets/screenshots/industrial_pipe.png",
                              "showcases/pipe_lab/rusted_pipe.astergraph"};
  brief.iteration_budget = 4;
  brief.minimum_score = 0.88f;
  return brief;
}

std::string makeAsterAgentAssetPrompt(const AsterAgentAssetBrief &brief) {
  std::ostringstream out;
  out << "You are producing an Aster-owned visual asset iteration.\n";
  out << "Brief: " << brief.id << "\n";
  out << "Target asset: " << brief.target_asset << "\n";
  out << "Title: " << brief.title << "\n";
  out << "Target: " << brief.target_description << "\n";
  out << "Iteration budget: " << brief.iteration_budget << "\n";
  out << "Minimum score: " << brief.minimum_score << "\n\n";
  out << "Reference images:\n";
  for (const AsterAgentAssetReference &reference : brief.references) {
    out << "- " << pathText(reference.path)
        << " role=" << asterAgentAssetReferenceRoleName(reference.role)
        << " weight=" << reference.weight << ": " << reference.note << "\n";
  }
  out << "\nRequired visual signals:\n";
  out << signalListText(brief.required_signals);
  out << "\nForbidden signals:\n";
  out << signalListText(brief.forbidden_signals);
  out << "\nExpected artifacts:\n";
  for (const std::filesystem::path &artifact : brief.expected_artifacts) {
    out << "- " << pathText(artifact) << "\n";
  }
  out << "\nRules:\n";
  out << "- Use the reference image as a visual contract, not decoration.\n";
  out << "- Preserve required signals before adding creative variation.\n";
  out << "- Do not invent forbidden hardware or simplify the material into one smooth color.\n";
  out << "- Treat coplanar seams, floating welds, and knife-edge rims as geometry failures, not color issues.\n";
  out << "- If the candidate fails the brief, report needs_work and name the next edit.\n";
  out << "\nReturn this structured asset report:\n";
  out << asterAgentAssetOutputSchemaJson() << "\n";
  return out.str();
}

AsterAgentAssetReview reviewAsterAgentAssetIteration(const AsterAgentAssetBrief &brief,
                                                     const AsterAgentAssetIteration &iteration,
                                                     std::filesystem::path source_path) {
  AsterAgentAssetReview review;
  if (iteration.id.empty()) {
    addDiagnostic(review.diagnostics, DiagnosticSeverity::Error, source_path, "$.id",
                  "asset iteration id is required");
  }
  if (iteration.artifact.empty()) {
    addDiagnostic(review.diagnostics, DiagnosticSeverity::Error, source_path, "$.artifact",
                  "asset iteration artifact is required");
  }

  float possible_score = 0.0f;
  float earned_score = 0.0f;
  for (const AsterAgentAssetQualitySignal &signal : brief.required_signals) {
    possible_score += signal.weight;
    if (iterationClaimsSignal(iteration, signal.id)) {
      earned_score += signal.weight;
    } else {
      review.missing_required_signals.push_back(signal.id);
      review.next_actions.push_back("restore required signal: " + signal.id);
    }
  }
  for (const AsterAgentAssetQualitySignal &signal : brief.forbidden_signals) {
    if (iterationClaimsSignal(iteration, signal.id) && !iterationRejectsSignal(iteration, signal.id)) {
      review.present_forbidden_signals.push_back(signal.id);
      review.next_actions.push_back("remove forbidden signal: " + signal.id);
    }
  }
  review.score = possible_score <= 0.0f ? 1.0f : earned_score / possible_score;
  if (!review.diagnostics.empty()) {
    review.status = AsterAgentAssetReviewStatus::Blocked;
  } else if (review.score >= brief.minimum_score && review.missing_required_signals.empty() &&
             review.present_forbidden_signals.empty()) {
    review.status = AsterAgentAssetReviewStatus::Passed;
  } else {
    review.status = AsterAgentAssetReviewStatus::NeedsWork;
  }
  if (review.status == AsterAgentAssetReviewStatus::NeedsWork && review.next_actions.empty()) {
    review.next_actions.push_back("compare candidate artifact against the reference image again");
  }
  return review;
}

std::string summarizeAsterAgentHandoffMarkdown(const AsterAgentHandoff &handoff) {
  std::ostringstream out;
  out << "# Aster Agent Handoff\n\n";
  out << "session: " << handoff.session_id << "\n\n";
  if (!handoff.summary.empty()) {
    out << handoff.summary << "\n\n";
  }
  if (!handoff.decisions.empty()) {
    out << "## Decisions\n";
    for (const std::string &decision : handoff.decisions) {
      out << "- " << decision << "\n";
    }
    out << "\n";
  }
  if (!handoff.changed_paths.empty()) {
    out << "## Changed Paths\n";
    for (const std::filesystem::path &path : handoff.changed_paths) {
      out << "- " << pathText(path) << "\n";
    }
    out << "\n";
  }
  if (!handoff.remaining_tasks.empty()) {
    out << "## Remaining Tasks\n";
    for (const AsterAgentTask &task_entry : handoff.remaining_tasks) {
      out << "- [" << asterAgentTaskStatusName(task_entry.status) << "] " << task_entry.id
          << ": " << task_entry.title << "\n";
    }
    out << "\n";
  }
  if (!handoff.diagnostics.empty()) {
    out << "## Diagnostics\n";
    for (const Diagnostic &diagnostic : handoff.diagnostics) {
      out << "- " << (diagnostic.severity == DiagnosticSeverity::Error ? "error" : "warning")
          << " " << diagnostic.path << ": " << diagnostic.message << "\n";
    }
  }
  return out.str();
}

} // namespace aster::sdk
