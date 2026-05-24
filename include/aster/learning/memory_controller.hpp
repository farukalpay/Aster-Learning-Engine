// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/core/world_state.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aster {

enum class MemoryActionKind : std::uint32_t {
  Read,
  Write,
  Evict,
  Replay,
  Scaffold,
  Stop,
};

enum class MemoryDecisionStatus : std::uint32_t {
  Accepted,
  Rejected,
  Blocked,
  ProviderError,
};

struct MemoryBudget {
  std::uint64_t token_budget = 0u;
  std::uint64_t byte_budget = 0u;
  double time_budget_ms = 0.0;
};

struct MemoryProviderConfig {
  std::string url;
  std::string method = "POST";
  std::string headers_json;
  std::string extra_json;
  std::uint32_t timeout_ms = 30000u;
  bool require_provider = false;
};

struct MemoryControllerOptions {
  std::string controller_id = "aster.memory_controller";
  std::filesystem::path store_path;
  std::string objective_id;
  MemoryBudget budget;
  MemoryProviderConfig provider;
  std::size_t trace_window = 16u;
};

struct MemoryControllerStepDesc {
  std::string task = "step";
  std::string subject;
  std::string semantic_key;
  MemoryBudget budget;
  std::vector<MemoryActionKind> allowed_actions;
};

struct MemoryDecision {
  std::uint64_t sequence = 0u;
  std::uint64_t tick = 0u;
  MemoryActionKind action = MemoryActionKind::Stop;
  MemoryDecisionStatus status = MemoryDecisionStatus::Accepted;
  std::string subject;
  std::string semantic_key;
  std::string rationale;
  std::string provider_status;
  std::string provider_response_json;
  std::string artifact_path;
  float confidence = 0.0f;
  float success_score = 0.0f;
  std::uint64_t token_cost = 0u;
  std::uint64_t byte_cost = 0u;
  std::uint64_t saved_bytes = 0u;
  std::uint64_t provider_status_code = 0u;
  std::uint64_t decision_hash = 0u;
};

struct MemoryGraphQueryDesc {
  std::filesystem::path store_path;
  std::string subject;
  std::string semantic_key;
  std::size_t limit = 16u;
};

struct MemoryGraphQueryResult {
  std::size_t node_count = 0u;
  std::size_t edge_count = 0u;
  std::size_t conflict_count = 0u;
  std::uint64_t query_hash = 0u;
  std::string json;
  std::string diagnostic;
};

struct MemoryBenchmarkReport {
  std::string suite_id;
  std::filesystem::path store_path;
  std::filesystem::path artifact_path;
  bool blocked = false;
  bool passed = false;
  std::size_t case_count = 0u;
  std::size_t ablation_count = 0u;
  std::size_t regression_replay_count = 0u;
  float score = 0.0f;
  std::uint64_t report_hash = 0u;
  std::string diagnostic;
};

[[nodiscard]] std::string_view memoryActionKindName(MemoryActionKind kind);
[[nodiscard]] std::string_view memoryDecisionStatusName(MemoryDecisionStatus status);

class MemoryController {
public:
  MemoryController() = default;
  explicit MemoryController(MemoryControllerOptions options);

  void setOptions(MemoryControllerOptions options);
  [[nodiscard]] const MemoryControllerOptions &options() const noexcept;

  [[nodiscard]] MemoryDecision step(WorldState &world, const MemoryControllerStepDesc &desc);
  [[nodiscard]] MemoryGraphQueryResult queryGraph(const MemoryGraphQueryDesc &desc) const;
  [[nodiscard]] MemoryBenchmarkReport benchmarkReport(std::string suite_id = {}) const;
  [[nodiscard]] const std::vector<MemoryDecision> &decisions() const noexcept;

private:
  [[nodiscard]] MemoryDecision reducerDecision(const WorldState &world,
                                               const MemoryControllerStepDesc &desc,
                                               const MemoryGraphQueryResult &graph) const;
  [[nodiscard]] MemoryDecision providerDecision(const WorldState &world,
                                                const MemoryControllerStepDesc &desc,
                                                const MemoryGraphQueryResult &graph) const;
  void persistTraceWindow(const WorldState &world) const;
  void persistDecision(const MemoryDecision &decision, std::string provider_json) const;

  MemoryControllerOptions options_{};
  std::vector<MemoryDecision> decisions_;
};

} // namespace aster
