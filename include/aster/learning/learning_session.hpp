// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/game_sdk/game_sdk.hpp"
#include "aster/learning/learning_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct LearningSignal {
  std::string event;
  sdk::AssetId asset;
  std::vector<std::string> channels;
  std::map<std::string, std::string> metadata;
  std::string stage = "teach";
};

struct LearningScaffoldCandidate {
  std::string scaffold_id;
  std::string stage;
  std::string intent;
  std::string intervention;
  std::string hypothesis_id;
  std::string misconception_id;
  std::vector<std::string> evidence_ids;
  std::string rationale;
};

struct LearningSessionReport {
  bool passed = false;
  bool evaluators_consistent = false;
  sdk::LearningProofReport authoring;
  LearningRuntimeReport runtime;
  std::vector<std::string> diagnostics;
  std::uint64_t session_stamp = 0u;
};

struct LearningSessionArtifacts {
  bool written = false;
  std::filesystem::path trace_path;
  std::filesystem::path report_path;
  std::string diagnostic;
};

class LearningSession {
public:
  LearningSession() = default;
  LearningSession(sdk::ProjectDocument project, sdk::LessonDocument lesson,
                  std::filesystem::path project_file = {});

  void reset();
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] const std::vector<sdk::Diagnostic> &diagnostics() const noexcept;
  [[nodiscard]] const sdk::ProjectDocument &project() const noexcept;
  [[nodiscard]] const sdk::LessonDocument &lesson() const noexcept;
  [[nodiscard]] const sdk::LearningTraceDocument &trace() const noexcept;

  void observe(const LearningSignal &signal);
  void observe(const std::vector<LearningSignal> &signals);
  [[nodiscard]] std::vector<LearningScaffoldCandidate> scaffoldCandidates() const;
  [[nodiscard]] bool selectScaffold(std::string_view scaffold_id);
  [[nodiscard]] bool claimMastery();
  [[nodiscard]] LearningSessionReport evaluate() const;
  [[nodiscard]] LearningSessionArtifacts
  writeArtifacts(const std::filesystem::path &output_dir) const;
  [[nodiscard]] std::vector<std::string> statusLines() const;

private:
  struct EvidenceProgress {
    std::set<std::string> events;
    std::set<std::string> channels;
    bool emitted = false;
  };

  [[nodiscard]] const sdk::LearningObjectiveDocument *
  objectiveForEvidence(std::string_view evidence_id) const;
  [[nodiscard]] bool objectiveEvidenceComplete() const;
  void recordEvent(sdk::LearningTraceEvent event);

  sdk::ProjectDocument project_;
  sdk::LessonDocument lesson_;
  sdk::LearningTraceDocument trace_;
  std::filesystem::path project_file_;
  std::vector<sdk::Diagnostic> diagnostics_;
  std::map<std::string, EvidenceProgress> evidence_progress_;
  std::set<std::string> selected_scaffolds_;
  std::set<std::string> diagnosed_misconceptions_;
  std::uint64_t next_timestamp_ = 1u;
};

struct LearningSessionLoadResult {
  LearningSession session;
  std::vector<sdk::Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const;
};

[[nodiscard]] LearningSessionLoadResult
loadLearningSession(const std::filesystem::path &project_file, std::string_view lesson_id = {});

} // namespace aster
