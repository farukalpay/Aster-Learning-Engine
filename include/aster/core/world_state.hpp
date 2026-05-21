// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct WorldEntityHandle {
  std::uint64_t id = 0u;
  std::uint32_t generation = 0u;

  [[nodiscard]] bool valid() const noexcept {
    return id != 0u && generation != 0u;
  }

  [[nodiscard]] friend bool operator==(const WorldEntityHandle lhs,
                                       const WorldEntityHandle rhs) noexcept {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
};

enum class WorldComponentAccessMode : std::uint32_t {
  Read,
  Write,
};

struct WorldComponentAccess {
  std::string component;
  std::string subject;
  WorldComponentAccessMode mode = WorldComponentAccessMode::Read;
};

enum class WorldTraceEventKind : std::uint32_t {
  InputEvent,
  SimulationTick,
  SchedulerDecision,
  AssetResolution,
  ResidencyDecision,
  RenderableExtraction,
  FrameSubmission,
  TransactionBegin,
  TransactionCommit,
  TransactionAbort,
  EntityCreated,
  EntityDestroyed,
  SnapshotSaved,
  SnapshotLoaded,
  Migration,
  Replay,
  ValidationError,
};

struct WorldTraceEvent {
  WorldTraceEventKind kind = WorldTraceEventKind::SimulationTick;
  std::uint64_t sequence = 0u;
  std::uint64_t tick = 0u;
  std::uint64_t transaction_id = 0u;
  WorldEntityHandle entity{};
  std::string label;
  std::string subject;
  std::string component;
  std::string detail;
  std::uint64_t parent_world_hash = 0u;
  std::uint64_t world_hash = 0u;
  std::uint64_t trace_hash = 0u;
};

struct WorldTickDesc {
  std::uint64_t tick = 0u;
  double delta_seconds = 0.0;
  std::uint64_t input_event_hash = 0u;
  std::uint64_t asset_lineage_hash = 0u;
  std::uint64_t extraction_hash = 0u;
  std::uint64_t frame_submission_hash = 0u;
};

struct WorldTickResult {
  bool accepted = false;
  std::uint64_t tick = 0u;
  double time_seconds = 0.0;
  std::size_t events_emitted = 0u;
  std::uint64_t world_hash = 0u;
  std::uint64_t trace_hash = 0u;
  std::string diagnostic;
};

struct WorldTransactionDesc {
  std::string label;
  std::string provenance;
  std::vector<WorldComponentAccess> accesses;
};

struct WorldTransactionInfo {
  std::uint64_t transaction_id = 0u;
  bool committed = false;
  std::size_t access_count = 0u;
  std::uint64_t parent_world_hash = 0u;
  std::uint64_t post_world_hash = 0u;
  std::uint64_t deterministic_stamp = 0u;
  std::string diagnostic;
};

struct WorldTraceCounts {
  std::size_t event_count = 0u;
  std::size_t entity_count = 0u;
  std::size_t live_entity_count = 0u;
  std::size_t transaction_count = 0u;
  std::size_t validation_event_count = 0u;
  std::uint64_t tick = 0u;
  double time_seconds = 0.0;
  std::uint64_t world_hash = 0u;
  std::uint64_t trace_hash = 0u;
};

enum class ResidencyDecisionKind : std::uint32_t {
  Keep,
  Load,
  Evict,
  Reject,
};

struct ResidencyBudget {
  std::uint64_t byte_budget = 0u;
};

struct ResidencyAsset {
  std::string asset_id;
  std::uint64_t byte_cost = 0u;
  float priority = 0.0f;
  bool visible = false;
  bool resident = false;
};

struct ResidencyDecision {
  std::string asset_id;
  ResidencyDecisionKind decision = ResidencyDecisionKind::Reject;
  std::uint64_t byte_cost = 0u;
  float priority = 0.0f;
  bool visible = false;
  std::string reason;
};

struct WorldMigrationReport {
  std::uint32_t loaded_schema_version = 0u;
  std::uint32_t current_schema_version = 1u;
  bool migration_applied = false;
  std::size_t entity_count = 0u;
  std::uint64_t world_hash = 0u;
  std::uint64_t trace_hash = 0u;
  std::string diagnostic;
};

struct WorldReplayReport {
  bool matched = false;
  std::size_t events_replayed = 0u;
  std::uint64_t expected_world_hash = 0u;
  std::uint64_t actual_world_hash = 0u;
  std::uint64_t actual_trace_hash = 0u;
  std::string diagnostic;
};

struct WorldStateConfig {
  double fixed_step_seconds = 1.0 / 60.0;
  std::uint64_t seed = 0xA57E570A5EEDull;
  std::string label;
};

class WorldState {
public:
  explicit WorldState(WorldStateConfig config = {});

  [[nodiscard]] WorldEntityHandle createEntity(std::string label = {});
  [[nodiscard]] bool destroyEntity(WorldEntityHandle handle, std::string reason = {});
  [[nodiscard]] bool contains(WorldEntityHandle handle) const;
  [[nodiscard]] std::optional<std::string_view> entityLabel(WorldEntityHandle handle) const;

  [[nodiscard]] WorldTickResult tick(WorldTickDesc desc);

  [[nodiscard]] WorldTransactionInfo beginTransaction(WorldTransactionDesc desc);
  [[nodiscard]] bool appendAccess(std::uint64_t transaction_id, WorldComponentAccess access,
                                  std::string *diagnostic = nullptr);
  [[nodiscard]] WorldTransactionInfo commitTransaction(std::uint64_t transaction_id);
  [[nodiscard]] WorldTransactionInfo abortTransaction(std::uint64_t transaction_id,
                                                     std::string reason = {});

  [[nodiscard]] WorldTraceCounts counts() const;
  [[nodiscard]] const std::vector<WorldTraceEvent> &traceEvents() const noexcept;
  [[nodiscard]] std::vector<WorldTraceEvent> validationEvents() const;

  [[nodiscard]] bool saveSnapshot(const std::filesystem::path &path, std::string *diagnostic);
  [[nodiscard]] WorldMigrationReport loadSnapshot(const std::filesystem::path &path);
  [[nodiscard]] WorldReplayReport replaySnapshot(const std::filesystem::path &path,
                                                 std::uint64_t expected_world_hash);

  [[nodiscard]] std::vector<ResidencyDecision>
  planResidency(ResidencyBudget budget, const std::vector<ResidencyAsset> &assets);

  [[nodiscard]] std::uint64_t currentTick() const noexcept;
  [[nodiscard]] double timeSeconds() const noexcept;
  [[nodiscard]] std::uint64_t worldHash() const noexcept;
  [[nodiscard]] std::uint64_t traceHash() const noexcept;

private:
  struct EntityRecord {
    WorldEntityHandle handle{};
    std::string label;
    bool alive = false;
  };

  struct TransactionRecord {
    WorldTransactionInfo info{};
    std::string label;
    std::string provenance;
    std::vector<WorldComponentAccess> accesses;
    bool active = false;
  };

  [[nodiscard]] EntityRecord *entityRecord(WorldEntityHandle handle);
  [[nodiscard]] const EntityRecord *entityRecord(WorldEntityHandle handle) const;
  [[nodiscard]] TransactionRecord *transactionRecord(std::uint64_t transaction_id);
  [[nodiscard]] const TransactionRecord *transactionRecord(std::uint64_t transaction_id) const;
  [[nodiscard]] bool hasAccessHazard(const WorldComponentAccess &access,
                                     WorldComponentAccess *conflict) const;
  void appendEvent(WorldTraceEvent event);
  void appendValidation(std::string label, std::string detail);
  void mixWorld(std::string_view tag, std::uint64_t value);
  void mixWorld(std::string_view tag, std::string_view value);

  WorldStateConfig config_{};
  std::uint64_t current_tick_ = 0u;
  double time_seconds_ = 0.0;
  std::uint64_t world_hash_ = 0u;
  std::uint64_t trace_hash_ = 0u;
  std::uint64_t next_entity_id_ = 1u;
  std::uint64_t next_transaction_id_ = 1u;
  std::uint64_t next_event_sequence_ = 1u;
  std::vector<EntityRecord> entities_;
  std::vector<TransactionRecord> transactions_;
  std::vector<WorldComponentAccess> committed_accesses_this_tick_;
  std::vector<WorldTraceEvent> trace_events_;
};

[[nodiscard]] const char *worldComponentAccessModeName(WorldComponentAccessMode mode);
[[nodiscard]] const char *worldTraceEventKindName(WorldTraceEventKind kind);
[[nodiscard]] const char *residencyDecisionKindName(ResidencyDecisionKind kind);

} // namespace aster
