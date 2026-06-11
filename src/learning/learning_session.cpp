// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/learning/learning_session.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aster {
namespace {

[[nodiscard]] bool contains(const std::vector<std::string> &values, const std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

[[nodiscard]] bool hasErrors(const std::vector<sdk::Diagnostic> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(), [](const sdk::Diagnostic &diagnostic) {
    return diagnostic.severity == sdk::DiagnosticSeverity::Error;
  });
}

void appendDiagnostics(std::vector<sdk::Diagnostic> &out,
                       const std::vector<sdk::Diagnostic> &diagnostics) {
  out.insert(out.end(), diagnostics.begin(), diagnostics.end());
}

[[nodiscard]] std::string jsonEscape(const std::string_view value) {
  std::ostringstream out;
  for (const unsigned char character : value) {
    switch (character) {
    case '"':
      out << "\\\"";
      break;
    case '\\':
      out << "\\\\";
      break;
    case '\b':
      out << "\\b";
      break;
    case '\f':
      out << "\\f";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (character < 0x20u) {
        out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<unsigned int>(character) << std::dec;
      } else {
        out << static_cast<char>(character);
      }
      break;
    }
  }
  return out.str();
}

void writeStringArray(std::ostream &out, const std::vector<std::string> &values) {
  out << '[';
  for (std::size_t index = 0u; index < values.size(); ++index) {
    if (index > 0u) {
      out << ',';
    }
    out << '"' << jsonEscape(values[index]) << '"';
  }
  out << ']';
}

[[nodiscard]] LearningRuntimeContract runtimeContract(const sdk::LessonDocument &lesson) {
  LearningRuntimeContract contract;
  contract.lesson_id = lesson.id;
  contract.workflow_stages = lesson.workflow_stages;
  for (const sdk::LearningObjectiveDocument &objective : lesson.objectives) {
    contract.objectives.push_back({objective.id, objective.evidence_ids});
  }
  for (const sdk::LearningScaffoldRuleDocument &rule : lesson.scaffold_rules) {
    contract.scaffold_rules.push_back(
        {rule.id, rule.stage, rule.evidence_ids, rule.hypothesis_ids, rule.force_gameplay_change});
  }
  for (const sdk::PedagogicalSafetyCheckDocument &check : lesson.safety_checks) {
    for (const std::string &evidence : check.required_evidence_ids) {
      if (!contains(contract.safety_evidence_ids, evidence)) {
        contract.safety_evidence_ids.push_back(evidence);
      }
    }
  }
  return contract;
}

[[nodiscard]] LearningTraceForest runtimeForest(const sdk::LearningTraceDocument &trace) {
  LearningTraceForest forest;
  for (const sdk::LearningTraceEvent &event : trace.events) {
    forest.addEvent({.id = event.id,
                     .stage = event.stage,
                     .evidence_id = event.evidence_id,
                     .hypothesis_id = event.hypothesis_id,
                     .scaffold_id = event.scaffold_id,
                     .metadata = event.metadata,
                     .claims_mastery = event.claims_mastery});
  }
  return forest;
}

[[nodiscard]] std::filesystem::path resolveAssetPath(const std::filesystem::path &project_file,
                                                     const sdk::ProjectAssetRef &asset) {
  if (asset.path.is_absolute()) {
    return asset.path;
  }
  return project_file.parent_path() / asset.path;
}

} // namespace

LearningSession::LearningSession(sdk::ProjectDocument project, sdk::LessonDocument lesson,
                                 std::filesystem::path project_file)
    : project_(std::move(project)), lesson_(std::move(lesson)),
      project_file_(std::move(project_file)) {
  diagnostics_ = sdk::validateLessonDocument(lesson_, &project_, project_file_);
  reset();
}

void LearningSession::reset() {
  trace_ = {.schema_version = sdk::kAuthoringSchemaVersion,
            .id = "trace.live." + lesson_.id,
            .lesson = lesson_.id,
            .events = {}};
  evidence_progress_.clear();
  for (const sdk::LearningEvidenceRefDocument &evidence : lesson_.evidence_refs) {
    evidence_progress_.emplace(evidence.id, EvidenceProgress{});
  }
  selected_scaffolds_.clear();
  diagnosed_misconceptions_.clear();
  next_timestamp_ = 1u;
}

bool LearningSession::valid() const noexcept {
  return !lesson_.id.empty() && !project_.name.empty() && !hasErrors(diagnostics_);
}

const std::vector<sdk::Diagnostic> &LearningSession::diagnostics() const noexcept {
  return diagnostics_;
}

const sdk::ProjectDocument &LearningSession::project() const noexcept {
  return project_;
}

const sdk::LessonDocument &LearningSession::lesson() const noexcept {
  return lesson_;
}

const sdk::LearningTraceDocument &LearningSession::trace() const noexcept {
  return trace_;
}

const sdk::LearningObjectiveDocument *
LearningSession::objectiveForEvidence(const std::string_view evidence_id) const {
  const auto objective = std::find_if(
      lesson_.objectives.begin(), lesson_.objectives.end(),
      [&](const auto &candidate) { return contains(candidate.evidence_ids, evidence_id); });
  return objective == lesson_.objectives.end() ? nullptr : &*objective;
}

void LearningSession::recordEvent(sdk::LearningTraceEvent event) {
  event.timestamp = next_timestamp_++;
  if (event.id.empty()) {
    event.id = "live." + std::to_string(event.timestamp);
  }
  trace_.events.push_back(std::move(event));
}

void LearningSession::observe(const LearningSignal &signal) {
  if (!valid() || signal.event.empty()) {
    return;
  }

  for (const sdk::LearningMisconceptionDocument &misconception : lesson_.misconceptions) {
    if (!contains(misconception.trigger_events, signal.event) ||
        diagnosed_misconceptions_.contains(misconception.id)) {
      continue;
    }
    const auto hypothesis = std::find_if(
        lesson_.learner_state_hypotheses.begin(), lesson_.learner_state_hypotheses.end(),
        [&](const sdk::LearnerStateHypothesisDocument &candidate) {
          return contains(candidate.misconception_ids, misconception.id);
        });
    sdk::LearningTraceEvent event;
    event.stage = "diagnose";
    event.event = signal.event;
    event.misconception_id = misconception.id;
    event.metadata = signal.metadata;
    if (hypothesis != lesson_.learner_state_hypotheses.end()) {
      event.hypothesis_id = hypothesis->id;
      if (!hypothesis->evidence_ids.empty()) {
        event.evidence_id = hypothesis->evidence_ids.front();
      }
    }
    recordEvent(std::move(event));
    diagnosed_misconceptions_.insert(misconception.id);
  }

  for (const sdk::LearningEvidenceRefDocument &evidence : lesson_.evidence_refs) {
    if (!evidence.asset.empty() && signal.asset != evidence.asset) {
      continue;
    }
    EvidenceProgress &progress = evidence_progress_[evidence.id];
    if (contains(evidence.required_events, signal.event)) {
      progress.events.insert(signal.event);
    }
    progress.channels.insert(signal.channels.begin(), signal.channels.end());
    const bool events_complete =
        std::all_of(evidence.required_events.begin(), evidence.required_events.end(),
                    [&](const std::string &event) { return progress.events.contains(event); });
    const bool channels_complete = std::all_of(
        evidence.required_channels.begin(), evidence.required_channels.end(),
        [&](const std::string &channel) { return progress.channels.contains(channel); });
    if (!progress.emitted && events_complete && channels_complete) {
      sdk::LearningTraceEvent event;
      event.stage = signal.stage.empty() ? "teach" : signal.stage;
      event.event = signal.event;
      event.evidence_id = evidence.id;
      event.metadata = signal.metadata;
      if (const sdk::LearningObjectiveDocument *objective = objectiveForEvidence(evidence.id)) {
        event.objective_id = objective->id;
      }
      recordEvent(std::move(event));
      progress.emitted = true;
    }
  }
}

void LearningSession::observe(const std::vector<LearningSignal> &signals) {
  for (const LearningSignal &signal : signals) {
    observe(signal);
  }
}

std::vector<LearningScaffoldCandidate> LearningSession::scaffoldCandidates() const {
  std::vector<LearningScaffoldCandidate> candidates;
  for (const sdk::LearningScaffoldRuleDocument &rule : lesson_.scaffold_rules) {
    if (selected_scaffolds_.contains(rule.id)) {
      continue;
    }
    const auto misconception =
        std::find_if(rule.misconception_ids.begin(), rule.misconception_ids.end(),
                     [&](const std::string &id) { return diagnosed_misconceptions_.contains(id); });
    if (misconception == rule.misconception_ids.end()) {
      continue;
    }
    const auto hypothesis = std::find_if(
        rule.hypothesis_ids.begin(), rule.hypothesis_ids.end(), [&](const std::string &id) {
          return std::any_of(trace_.events.begin(), trace_.events.end(),
                             [&](const auto &event) { return event.hypothesis_id == id; });
        });
    const bool evidence_ready =
        std::all_of(rule.evidence_ids.begin(), rule.evidence_ids.end(), [&](const std::string &id) {
          return std::any_of(trace_.events.begin(), trace_.events.end(),
                             [&](const auto &event) { return event.evidence_id == id; });
        });
    if (hypothesis == rule.hypothesis_ids.end() || !evidence_ready) {
      continue;
    }
    std::ostringstream rationale;
    if (!rule.evidence_ids.empty()) {
      rationale << rule.evidence_ids.front();
    }
    if (!rule.hypothesis_ids.empty()) {
      rationale << " supports " << rule.hypothesis_ids.front();
    }
    candidates.push_back({.scaffold_id = rule.id,
                          .stage = rule.stage,
                          .intent = rule.intent,
                          .intervention = rule.intervention,
                          .hypothesis_id = *hypothesis,
                          .misconception_id = *misconception,
                          .evidence_ids = rule.evidence_ids,
                          .rationale = rationale.str()});
  }
  return candidates;
}

bool LearningSession::selectScaffold(const std::string_view scaffold_id) {
  const std::vector<LearningScaffoldCandidate> candidates = scaffoldCandidates();
  const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&](const auto &value) {
    return value.scaffold_id == scaffold_id;
  });
  if (candidate == candidates.end()) {
    return false;
  }
  recordEvent({.stage = candidate->stage,
               .event = "select_scaffold",
               .hypothesis_id = candidate->hypothesis_id,
               .scaffold_id = candidate->scaffold_id,
               .misconception_id = candidate->misconception_id,
               .metadata = {{"rationale", candidate->rationale},
                            {"intervention", candidate->intervention}}});
  selected_scaffolds_.insert(candidate->scaffold_id);
  return true;
}

bool LearningSession::objectiveEvidenceComplete() const {
  return std::all_of(
      lesson_.objectives.begin(), lesson_.objectives.end(), [&](const auto &objective) {
        return std::all_of(objective.evidence_ids.begin(), objective.evidence_ids.end(),
                           [&](const std::string &evidence) {
                             const auto progress = evidence_progress_.find(evidence);
                             return progress != evidence_progress_.end() &&
                                    progress->second.emitted;
                           });
      });
}

bool LearningSession::claimMastery() {
  if (!objectiveEvidenceComplete() ||
      std::any_of(trace_.events.begin(), trace_.events.end(),
                  [](const sdk::LearningTraceEvent &event) { return event.claims_mastery; })) {
    return false;
  }
  recordEvent({.stage = "evaluate", .event = "claim_mastery", .claims_mastery = true});
  return true;
}

LearningSessionReport LearningSession::evaluate() const {
  LearningSessionReport report;
  report.authoring = sdk::evaluateLearningTrace(lesson_, trace_, project_file_);
  report.runtime = evaluateLearningRuntime(runtimeContract(lesson_), runtimeForest(trace_));
  constexpr float kCoverageTolerance = 0.0001f;
  report.evaluators_consistent =
      report.authoring.passed == report.runtime.passed &&
      std::abs(report.authoring.objective_coverage - report.runtime.objective_coverage) <=
          kCoverageTolerance &&
      std::abs(report.authoring.workflow_coverage - report.runtime.workflow_coverage) <=
          kCoverageTolerance;
  if (!report.evaluators_consistent) {
    report.diagnostics.push_back("Game SDK and runtime learning evaluators disagree");
  }
  report.passed =
      valid() && report.authoring.passed && report.runtime.passed && report.evaluators_consistent;
  report.session_stamp =
      hashCombine64(report.authoring.contract_stamp, sdk::learningTraceContractStamp(trace_));
  return report;
}

LearningSessionArtifacts
LearningSession::writeArtifacts(const std::filesystem::path &output_dir) const {
  LearningSessionArtifacts artifacts;
  artifacts.trace_path = output_dir / "trace.jsonl";
  artifacts.report_path = output_dir / "proof.json";
  std::error_code error;
  std::filesystem::create_directories(output_dir, error);
  if (error) {
    artifacts.diagnostic = "failed to create learning output directory: " + error.message();
    return artifacts;
  }

  std::ofstream trace_out(artifacts.trace_path);
  if (!trace_out) {
    artifacts.diagnostic = "failed to open learning trace artifact";
    return artifacts;
  }
  for (const sdk::LearningTraceEvent &event : trace_.events) {
    trace_out << "{\"id\":\"" << jsonEscape(event.id) << "\",\"lesson\":\""
              << jsonEscape(lesson_.id) << "\",\"timestamp\":" << event.timestamp << ",\"stage\":\""
              << jsonEscape(event.stage) << "\",\"event\":\"" << jsonEscape(event.event) << '"';
    const auto field = [&](const char *name, const std::string &value) {
      if (!value.empty()) {
        trace_out << ",\"" << name << "\":\"" << jsonEscape(value) << '"';
      }
    };
    field("evidence_id", event.evidence_id);
    field("objective_id", event.objective_id);
    field("hypothesis_id", event.hypothesis_id);
    field("scaffold_id", event.scaffold_id);
    field("misconception_id", event.misconception_id);
    if (!event.metadata.empty()) {
      trace_out << ",\"metadata\":{";
      std::size_t index = 0u;
      for (const auto &[key, value] : event.metadata) {
        if (index++ > 0u) {
          trace_out << ',';
        }
        trace_out << '"' << jsonEscape(key) << "\":\"" << jsonEscape(value) << '"';
      }
      trace_out << '}';
    }
    if (event.claims_mastery) {
      trace_out << ",\"claims_mastery\":true";
    }
    trace_out << "}\n";
  }
  trace_out.close();

  const LearningSessionReport report = evaluate();
  std::ofstream report_out(artifacts.report_path);
  if (!report_out) {
    artifacts.diagnostic = "failed to open learning proof artifact";
    return artifacts;
  }
  report_out << "{\n  \"schema_version\": 1,\n"
             << "  \"lesson\": \"" << jsonEscape(lesson_.id) << "\",\n"
             << "  \"trace\": \"" << jsonEscape(artifacts.trace_path.filename().string()) << "\",\n"
             << "  \"passed\": " << (report.passed ? "true" : "false") << ",\n"
             << "  \"evaluators_consistent\": " << (report.evaluators_consistent ? "true" : "false")
             << ",\n"
             << "  \"event_count\": " << trace_.events.size() << ",\n"
             << "  \"objective_coverage\": " << report.authoring.objective_coverage << ",\n"
             << "  \"workflow_coverage\": " << report.authoring.workflow_coverage << ",\n"
             << "  \"runtime_objective_coverage\": " << report.runtime.objective_coverage << ",\n"
             << "  \"runtime_workflow_coverage\": " << report.runtime.workflow_coverage << ",\n"
             << "  \"contract_stamp\": " << report.authoring.contract_stamp << ",\n"
             << "  \"session_stamp\": " << report.session_stamp << ",\n"
             << "  \"missing_objectives\": ";
  writeStringArray(report_out, report.authoring.missing_objectives);
  report_out << ",\n  \"missing_workflow_stages\": ";
  writeStringArray(report_out, report.authoring.missing_workflow_stages);
  report_out << ",\n  \"missing_evidence\": ";
  writeStringArray(report_out, report.authoring.missing_evidence);
  report_out << ",\n  \"unsupported_interventions\": ";
  writeStringArray(report_out, report.authoring.unsupported_interventions);
  report_out << ",\n  \"safety_failures\": ";
  writeStringArray(report_out, report.authoring.safety_failures);
  report_out << "\n}\n";
  artifacts.written = static_cast<bool>(report_out);
  if (!artifacts.written) {
    artifacts.diagnostic = "failed to write learning proof artifact";
  }
  return artifacts;
}

std::vector<std::string> LearningSession::statusLines() const {
  const LearningSessionReport report = evaluate();
  std::vector<std::string> lines;
  std::ostringstream coverage;
  coverage << "learning: objectives "
           << static_cast<int>(report.authoring.objective_coverage * 100.0f) << "% / workflow "
           << static_cast<int>(report.authoring.workflow_coverage * 100.0f) << '%';
  lines.push_back(coverage.str());
  for (const LearningScaffoldCandidate &candidate : scaffoldCandidates()) {
    lines.push_back("scaffold: " + candidate.intervention);
  }
  if (report.passed) {
    lines.push_back("learning: proof passed; authoring/runtime evaluators agree");
  }
  return lines;
}

bool LearningSessionLoadResult::ok() const {
  return session.valid() && !hasErrors(diagnostics);
}

LearningSessionLoadResult loadLearningSession(const std::filesystem::path &project_file,
                                              const std::string_view lesson_id) {
  LearningSessionLoadResult result;
  const sdk::LoadResult<sdk::ProjectDocument> project = sdk::loadProjectDocument(project_file);
  appendDiagnostics(result.diagnostics, project.diagnostics);
  if (!project.ok()) {
    return result;
  }

  const auto lesson_asset = std::find_if(project.value.assets.begin(), project.value.assets.end(),
                                         [&](const sdk::ProjectAssetRef &asset) {
                                           return asset.kind == sdk::AssetKind::Lesson &&
                                                  (lesson_id.empty() || asset.id == lesson_id);
                                         });
  if (lesson_asset == project.value.assets.end()) {
    result.diagnostics.push_back(
        {.severity = sdk::DiagnosticSeverity::Error,
         .source = project_file,
         .path = "$.assets",
         .message = lesson_id.empty()
                        ? "project does not declare a lesson asset"
                        : "project does not declare lesson '" + std::string(lesson_id) + "'"});
    return result;
  }

  const std::filesystem::path lesson_path = resolveAssetPath(project_file, *lesson_asset);
  const sdk::LoadResult<sdk::LessonDocument> lesson = sdk::loadLessonDocument(lesson_path);
  appendDiagnostics(result.diagnostics, lesson.diagnostics);
  if (!lesson.ok()) {
    return result;
  }
  result.session = LearningSession(project.value, lesson.value, project_file);
  appendDiagnostics(result.diagnostics, result.session.diagnostics());
  return result;
}

} // namespace aster
