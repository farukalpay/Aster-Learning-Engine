// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/game_sdk/game_sdk.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace aster::sdk {

constexpr std::uint32_t kAsterAgentContractSchemaVersion = 1u;

enum class AsterAgentDomain {
  Unknown,
  Project,
  Scene,
  Prefab,
  Material,
  Item,
  ActionGraph,
  Geometry,
  Physics,
  Systems,
  Rendering,
  Ui,
  Build,
  Tests,
};

enum class AsterAgentTaskStatus {
  Planned,
  Ready,
  InProgress,
  Blocked,
  Complete,
};

enum class AsterAgentBatchPolicy {
  Serial,
  ParallelSafe,
  ReviewGate,
};

enum class AsterAgentAssetReferenceRole {
  VisualTarget,
  ShapeTarget,
  MaterialTarget,
  NegativeExample,
  CandidateOutput,
};

enum class AsterAgentAssetReviewStatus {
  NeedsWork,
  Passed,
  Blocked,
};

enum class AsterAgentCommandDecision {
  Allow,
  Review,
  Deny,
};

struct AsterAgentScopeRule {
  std::filesystem::path path;
  std::string owner;
  std::vector<AsterAgentDomain> domains;
  bool writable = true;
};

struct AsterAgentValidationCommand {
  std::string id;
  std::string command;
  std::filesystem::path working_directory;
  std::vector<AsterAgentDomain> domains;
  bool required = true;
};

struct AsterAgentOutputContract {
  std::string id;
  std::string description;
  std::vector<std::string> required_keys;
};

struct AsterAgentWorkspaceOptions {
  std::string objective;
  std::filesystem::path project_file;
  std::vector<std::filesystem::path> additional_roots;
  bool include_default_validation = true;
  bool allow_kernel_changes = false;
};

struct AsterAgentWorkspaceProfile {
  std::uint32_t schema_version = kAsterAgentContractSchemaVersion;
  std::string name;
  std::string objective;
  std::filesystem::path project_root;
  std::filesystem::path project_file;
  std::vector<std::filesystem::path> additional_roots;
  std::vector<AsterAgentScopeRule> scopes;
  std::vector<AsterAgentValidationCommand> validation;
  std::vector<AsterAgentOutputContract> output_contracts;
  std::map<std::string, std::string> metadata;
};

struct AsterAgentInstructionFile {
  std::filesystem::path path;
  std::filesystem::path scope_root;
  std::vector<std::string> rules;
  std::uint32_t depth = 0u;
};

struct AsterAgentInstructionOptions {
  std::filesystem::path workspace_root;
  std::string file_name = "AGENTS.md";
  std::uint32_t max_rule_chars = 240u;
};

struct AsterAgentInstructionStack {
  std::vector<AsterAgentInstructionFile> files;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const;
  [[nodiscard]] std::vector<std::string> flattenedRules() const;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string summarizeMarkdown() const;
};

struct AsterAgentAssetSignal {
  AssetId id;
  AssetKind kind = AssetKind::Unknown;
  std::filesystem::path path;
  std::vector<AsterAgentDomain> domains;
  bool startup = false;
};

struct AsterAgentWorkspaceAudit {
  std::vector<Diagnostic> diagnostics;
  std::vector<AsterAgentAssetSignal> assets;
  std::vector<std::string> recommended_batches;

  [[nodiscard]] bool ok() const;
};

struct AsterAgentTask {
  std::string id;
  std::string title;
  std::string brief;
  std::vector<AsterAgentDomain> domains;
  std::vector<std::string> depends_on;
  std::vector<std::filesystem::path> touched_paths;
  AsterAgentTaskStatus status = AsterAgentTaskStatus::Planned;
  int priority = 0;
  std::uint32_t estimated_steps = 1u;
};

struct AsterAgentBatch {
  std::string id;
  std::string title;
  AsterAgentBatchPolicy policy = AsterAgentBatchPolicy::Serial;
  std::vector<AsterAgentTask> tasks;
  std::vector<AsterAgentValidationCommand> validation;
};

struct AsterAgentHandoff {
  std::string session_id;
  std::string summary;
  std::vector<std::string> decisions;
  std::vector<std::filesystem::path> changed_paths;
  std::vector<AsterAgentTask> remaining_tasks;
  std::vector<Diagnostic> diagnostics;
};

struct AsterAgentAssetReference {
  std::filesystem::path path;
  AsterAgentAssetReferenceRole role = AsterAgentAssetReferenceRole::VisualTarget;
  std::string note;
  float weight = 1.0f;
};

struct AsterAgentAssetQualitySignal {
  std::string id;
  std::string description;
  float weight = 1.0f;
};

struct AsterAgentAssetBrief {
  std::string id;
  AssetId target_asset;
  std::string title;
  std::string target_description;
  std::vector<AsterAgentAssetReference> references;
  std::vector<AsterAgentAssetQualitySignal> required_signals;
  std::vector<AsterAgentAssetQualitySignal> forbidden_signals;
  std::vector<std::filesystem::path> expected_artifacts;
  int iteration_budget = 3;
  float minimum_score = 0.82f;
};

struct AsterAgentAssetPresentationQuality {
  std::string scale_cues;
  std::string contact_shadows;
  std::string surface_occlusion;
  std::string volumetric_depth;
  std::string camera_language;
  std::filesystem::path preview_artifact;
};

struct AsterAgentAssetSurfaceStack {
  float physical_texel_density = 0.0f;
  float height_normal_coupling = 0.0f;
  float roughness_height_coupling = 0.0f;
  float macro_frequency_breakup = 0.0f;
  float micro_frequency_breakup = 0.0f;
};

struct AsterAgentAssetIteration {
  std::string id;
  std::filesystem::path artifact;
  std::string notes;
  std::vector<std::string> claimed_signals;
  std::vector<std::string> rejected_signals;
  AsterAgentAssetPresentationQuality presentation_quality;
  AsterAgentAssetSurfaceStack surface_stack;
};

struct AsterAgentAssetReview {
  AsterAgentAssetReviewStatus status = AsterAgentAssetReviewStatus::NeedsWork;
  float score = 0.0f;
  std::vector<std::string> missing_required_signals;
  std::vector<std::string> present_forbidden_signals;
  std::vector<std::string> next_actions;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool passed() const noexcept {
    return status == AsterAgentAssetReviewStatus::Passed;
  }
};

struct AsterAgentCommandRule {
  std::string id;
  std::vector<std::string> prefix;
  AsterAgentCommandDecision decision = AsterAgentCommandDecision::Review;
  std::string rationale;
};

struct AsterAgentCommandReview {
  AsterAgentCommandDecision decision = AsterAgentCommandDecision::Review;
  std::string rule_id;
  std::string rationale;
  std::vector<std::string> command;
};

class AsterAgentCommandPolicy {
public:
  [[nodiscard]] bool addRule(AsterAgentCommandRule rule);
  [[nodiscard]] AsterAgentCommandReview reviewCommand(std::vector<std::string> command) const;
  [[nodiscard]] AsterAgentCommandReview reviewShellCommand(std::string_view command) const;

  [[nodiscard]] const std::vector<AsterAgentCommandRule> &rules() const noexcept;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string summarizeMarkdown() const;

private:
  std::vector<AsterAgentCommandRule> rules_;
};

struct AsterAgentRunbookOptions {
  std::filesystem::path workspace_root;
  bool include_instruction_files = true;
  bool include_default_command_policy = true;
  bool allow_kernel_changes = false;
};

struct AsterAgentRunbook {
  AsterAgentWorkspaceProfile profile;
  AsterAgentInstructionStack instructions;
  AsterAgentCommandPolicy command_policy;
  std::vector<std::string> notes;

  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string summarizeMarkdown() const;
};

class AsterAgentTaskBoard {
public:
  [[nodiscard]] bool addTask(AsterAgentTask task);
  [[nodiscard]] bool addBatch(AsterAgentBatch batch);
  [[nodiscard]] bool setStatus(std::string_view id, AsterAgentTaskStatus status);

  [[nodiscard]] const AsterAgentTask *task(std::string_view id) const;
  [[nodiscard]] std::vector<AsterAgentTask> tasks() const;
  [[nodiscard]] const std::vector<AsterAgentBatch> &batches() const noexcept;
  [[nodiscard]] std::vector<AsterAgentTask> readyTasks() const;
  [[nodiscard]] std::vector<AsterAgentTask> blockedTasks() const;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string summarizeMarkdown() const;

private:
  [[nodiscard]] bool dependencyComplete(std::string_view id) const;
  [[nodiscard]] bool dependenciesReady(const AsterAgentTask &task) const;
  [[nodiscard]] bool hasMissingDependency(const AsterAgentTask &task) const;

  std::vector<AsterAgentTask> tasks_;
  std::vector<AsterAgentBatch> batches_;
};

[[nodiscard]] std::string_view asterAgentDomainName(AsterAgentDomain domain);
[[nodiscard]] AsterAgentDomain parseAsterAgentDomain(std::string_view value);
[[nodiscard]] std::string_view asterAgentTaskStatusName(AsterAgentTaskStatus status);
[[nodiscard]] std::string_view asterAgentBatchPolicyName(AsterAgentBatchPolicy policy);
[[nodiscard]] std::string_view
asterAgentAssetReferenceRoleName(AsterAgentAssetReferenceRole role);
[[nodiscard]] std::string_view
asterAgentAssetReviewStatusName(AsterAgentAssetReviewStatus status);
[[nodiscard]] std::string_view asterAgentCommandDecisionName(AsterAgentCommandDecision decision);
[[nodiscard]] std::vector<AsterAgentDomain> asterAgentDomainsForAssetKind(AssetKind kind);

[[nodiscard]] AsterAgentInstructionStack
loadAsterAgentInstructions(const std::filesystem::path &start_path,
                           AsterAgentInstructionOptions options = {});

[[nodiscard]] AsterAgentWorkspaceProfile
createAsterAgentWorkspaceProfile(const ProjectDocument &project,
                                 const std::filesystem::path &project_root,
                                 AsterAgentWorkspaceOptions options = {});

[[nodiscard]] AsterAgentWorkspaceAudit
auditAsterAgentWorkspace(const ProjectDocument &project,
                         const AsterAgentWorkspaceProfile &profile);

[[nodiscard]] AsterAgentTaskBoard
planAsterAgentAuthoringBatches(const ProjectDocument &project,
                               const AsterAgentWorkspaceProfile &profile);

[[nodiscard]] AsterAgentCommandPolicy createDefaultAsterAgentCommandPolicy(
    bool allow_kernel_changes = false);
[[nodiscard]] AsterAgentRunbook
createAsterAgentRunbook(const ProjectDocument &project,
                        const std::filesystem::path &project_root,
                        AsterAgentRunbookOptions runbook_options = {},
                        AsterAgentWorkspaceOptions workspace_options = {});
[[nodiscard]] std::string asterAgentBatchOutputSchemaJson();
[[nodiscard]] std::string asterAgentAssetOutputSchemaJson();
[[nodiscard]] std::string makeAsterAgentPrompt(const AsterAgentWorkspaceProfile &profile,
                                               const ProjectDocument &project,
                                               const AsterAgentTaskBoard &board);
[[nodiscard]] std::string summarizeAsterAgentHandoffMarkdown(const AsterAgentHandoff &handoff);

[[nodiscard]] AsterAgentAssetBrief
makeIndustrialPipeAssetBrief(std::filesystem::path reference_image,
                             AssetId target_asset = "asset_graph.pipe_lab.rusted_pipe");
[[nodiscard]] std::string makeAsterAgentAssetPrompt(const AsterAgentAssetBrief &brief);
[[nodiscard]] AsterAgentAssetReview
reviewAsterAgentAssetIteration(const AsterAgentAssetBrief &brief,
                               const AsterAgentAssetIteration &iteration,
                               std::filesystem::path source_path = {});

} // namespace aster::sdk
