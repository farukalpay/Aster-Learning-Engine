// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/core/world_state.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace aster {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
constexpr std::uint32_t kWorldSnapshotSchemaVersion = 1u;

[[nodiscard]] std::uint64_t fnvAppend(std::uint64_t hash, const std::uint64_t value) {
  for (std::size_t byte = 0u; byte < sizeof(value); ++byte) {
    hash ^= (value >> (byte * 8u)) & 0xffu;
    hash *= kFnvPrime;
  }
  return hash;
}

[[nodiscard]] std::uint64_t fnvAppend(std::uint64_t hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= kFnvPrime;
  }
  return hash;
}

[[nodiscard]] std::uint64_t fnvAppendDouble(std::uint64_t hash, const double value) {
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  std::uint64_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(bits));
  return fnvAppend(hash, bits);
}

[[nodiscard]] std::uint64_t accessStamp(const WorldComponentAccess &access) {
  std::uint64_t hash = kFnvOffset;
  hash = fnvAppend(hash, worldComponentAccessModeName(access.mode));
  hash = fnvAppend(hash, access.subject);
  hash = fnvAppend(hash, access.component);
  return hash;
}

[[nodiscard]] bool accessSameResource(const WorldComponentAccess &lhs,
                                      const WorldComponentAccess &rhs) {
  return lhs.subject == rhs.subject && lhs.component == rhs.component;
}

[[nodiscard]] bool accessConflicts(const WorldComponentAccess &lhs,
                                   const WorldComponentAccess &rhs) {
  return accessSameResource(lhs, rhs) &&
         (lhs.mode == WorldComponentAccessMode::Write ||
          rhs.mode == WorldComponentAccessMode::Write);
}

[[nodiscard]] std::string hexHash(const std::uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

[[nodiscard]] std::vector<std::string> split(const std::string_view text, const char delimiter) {
  std::vector<std::string> out;
  std::size_t begin = 0u;
  while (begin <= text.size()) {
    const std::size_t end = text.find(delimiter, begin);
    if (end == std::string_view::npos) {
      out.emplace_back(text.substr(begin));
      break;
    }
    out.emplace_back(text.substr(begin, end - begin));
    begin = end + 1u;
  }
  return out;
}

template <typename T> [[nodiscard]] bool parseInteger(const std::string_view text, T &out) {
  const char *begin = text.data();
  const char *end = begin + text.size();
  const auto result = std::from_chars(begin, end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

} // namespace

WorldState::WorldState(WorldStateConfig config) : config_(std::move(config)) {
  world_hash_ = kFnvOffset;
  world_hash_ = fnvAppend(world_hash_, "aster.world.v1");
  world_hash_ = fnvAppend(world_hash_, config_.seed);
  world_hash_ = fnvAppend(world_hash_, config_.label);
  trace_hash_ = fnvAppend(kFnvOffset, "aster.world.trace.v1");
}

WorldEntityHandle WorldState::createEntity(std::string label) {
  EntityRecord record;
  record.handle = {.id = next_entity_id_++, .generation = 1u};
  record.label = std::move(label);
  record.alive = true;
  const WorldEntityHandle handle = record.handle;
  entities_.push_back(std::move(record));

  mixWorld("entity.create", handle.id);
  mixWorld("entity.generation", handle.generation);
  mixWorld("entity.label", entities_.back().label);
  appendEvent({.kind = WorldTraceEventKind::EntityCreated,
               .tick = current_tick_,
               .entity = handle,
               .label = entities_.back().label,
               .detail = "created",
               .world_hash = world_hash_});
  return handle;
}

bool WorldState::destroyEntity(const WorldEntityHandle handle, std::string reason) {
  EntityRecord *record = entityRecord(handle);
  if (record == nullptr || !record->alive) {
    appendValidation("entity.destroy", "stale or unknown entity handle");
    return false;
  }
  record->alive = false;
  ++record->handle.generation;
  mixWorld("entity.destroy", handle.id);
  mixWorld("entity.destroy.generation", record->handle.generation);
  mixWorld("entity.destroy.reason", reason);
  appendEvent({.kind = WorldTraceEventKind::EntityDestroyed,
               .tick = current_tick_,
               .entity = handle,
               .label = record->label,
               .detail = reason.empty() ? "destroyed" : std::move(reason),
               .world_hash = world_hash_});
  return true;
}

bool WorldState::contains(const WorldEntityHandle handle) const {
  const EntityRecord *record = entityRecord(handle);
  return record != nullptr && record->alive;
}

std::optional<std::string_view> WorldState::entityLabel(const WorldEntityHandle handle) const {
  const EntityRecord *record = entityRecord(handle);
  if (record == nullptr || !record->alive) {
    return std::nullopt;
  }
  return std::string_view(record->label);
}

WorldTickResult WorldState::tick(WorldTickDesc desc) {
  if (desc.tick == 0u) {
    desc.tick = current_tick_ + 1u;
  }
  if (desc.tick <= current_tick_) {
    appendValidation("simulation.tick", "tick must be strictly monotonic");
    return {.accepted = false,
            .tick = current_tick_,
            .time_seconds = time_seconds_,
            .world_hash = world_hash_,
            .trace_hash = trace_hash_,
            .diagnostic = "tick must be strictly monotonic"};
  }
  if (!std::isfinite(desc.delta_seconds) || desc.delta_seconds <= 0.0) {
    desc.delta_seconds = config_.fixed_step_seconds > 0.0 ? config_.fixed_step_seconds
                                                          : 1.0 / 60.0;
  }
  const std::size_t before = trace_events_.size();
  current_tick_ = desc.tick;
  time_seconds_ += desc.delta_seconds;
  committed_accesses_this_tick_.clear();

  mixWorld("tick", current_tick_);
  world_hash_ = fnvAppendDouble(world_hash_, desc.delta_seconds);
  mixWorld("input", desc.input_event_hash);
  appendEvent({.kind = WorldTraceEventKind::InputEvent,
               .tick = current_tick_,
               .label = "input",
               .detail = hexHash(desc.input_event_hash),
               .world_hash = world_hash_});
  appendEvent({.kind = WorldTraceEventKind::SimulationTick,
               .tick = current_tick_,
               .label = "simulation",
               .detail = "dt=" + std::to_string(desc.delta_seconds),
               .world_hash = world_hash_});
  if (desc.asset_lineage_hash != 0u) {
    mixWorld("asset.lineage", desc.asset_lineage_hash);
    appendEvent({.kind = WorldTraceEventKind::AssetResolution,
                 .tick = current_tick_,
                 .label = "asset-lineage",
                 .detail = hexHash(desc.asset_lineage_hash),
                 .world_hash = world_hash_});
  }
  if (desc.extraction_hash != 0u) {
    mixWorld("render.extraction", desc.extraction_hash);
    appendEvent({.kind = WorldTraceEventKind::RenderableExtraction,
                 .tick = current_tick_,
                 .label = "renderable-extraction",
                 .detail = hexHash(desc.extraction_hash),
                 .world_hash = world_hash_});
  }
  if (desc.frame_submission_hash != 0u) {
    mixWorld("frame.submission", desc.frame_submission_hash);
    appendEvent({.kind = WorldTraceEventKind::FrameSubmission,
                 .tick = current_tick_,
                 .label = "frame-submission",
                 .detail = hexHash(desc.frame_submission_hash),
                 .world_hash = world_hash_});
  }

  return {.accepted = true,
          .tick = current_tick_,
          .time_seconds = time_seconds_,
          .events_emitted = trace_events_.size() - before,
          .world_hash = world_hash_,
          .trace_hash = trace_hash_};
}

WorldTransactionInfo WorldState::beginTransaction(WorldTransactionDesc desc) {
  TransactionRecord record;
  record.info.transaction_id = next_transaction_id_++;
  record.info.access_count = desc.accesses.size();
  record.info.parent_world_hash = world_hash_;
  record.info.post_world_hash = world_hash_;
  record.info.deterministic_stamp = fnvAppend(kFnvOffset, record.info.transaction_id);
  record.label = std::move(desc.label);
  record.provenance = std::move(desc.provenance);
  record.accesses = std::move(desc.accesses);
  record.active = true;
  for (const WorldComponentAccess &access : record.accesses) {
    record.info.deterministic_stamp =
        fnvAppend(record.info.deterministic_stamp, accessStamp(access));
  }
  WorldTransactionInfo info = record.info;
  transactions_.push_back(std::move(record));
  appendEvent({.kind = WorldTraceEventKind::TransactionBegin,
               .tick = current_tick_,
               .transaction_id = info.transaction_id,
               .label = transactions_.back().label,
               .detail = transactions_.back().provenance,
               .parent_world_hash = info.parent_world_hash,
               .world_hash = world_hash_});
  return info;
}

bool WorldState::appendAccess(const std::uint64_t transaction_id, WorldComponentAccess access,
                              std::string *diagnostic) {
  TransactionRecord *record = transactionRecord(transaction_id);
  if (record == nullptr || !record->active) {
    if (diagnostic != nullptr) {
      *diagnostic = "transaction is not active";
    }
    appendValidation("transaction.append", "transaction is not active");
    return false;
  }
  if (access.component.empty() || access.subject.empty()) {
    if (diagnostic != nullptr) {
      *diagnostic = "component access requires component and subject";
    }
    appendValidation("transaction.append", "component access requires component and subject");
    return false;
  }
  record->info.deterministic_stamp = fnvAppend(record->info.deterministic_stamp, accessStamp(access));
  record->accesses.push_back(std::move(access));
  record->info.access_count = record->accesses.size();
  return true;
}

WorldTransactionInfo WorldState::commitTransaction(const std::uint64_t transaction_id) {
  TransactionRecord *record = transactionRecord(transaction_id);
  if (record == nullptr || !record->active) {
    appendValidation("transaction.commit", "transaction is not active");
    return {.transaction_id = transaction_id, .diagnostic = "transaction is not active"};
  }

  for (const WorldComponentAccess &access : record->accesses) {
    WorldComponentAccess conflict;
    if (hasAccessHazard(access, &conflict)) {
      record->active = false;
      record->info.committed = false;
      record->info.diagnostic = "component access hazard: " + access.subject + "." +
                                access.component + " conflicts with prior " +
                                worldComponentAccessModeName(conflict.mode);
      appendEvent({.kind = WorldTraceEventKind::SchedulerDecision,
                   .tick = current_tick_,
                   .transaction_id = record->info.transaction_id,
                   .label = record->label,
                   .subject = access.subject,
                   .component = access.component,
                   .detail = record->info.diagnostic,
                   .parent_world_hash = record->info.parent_world_hash,
                   .world_hash = world_hash_});
      appendValidation("scheduler.hazard", record->info.diagnostic);
      return record->info;
    }
  }

  record->info.parent_world_hash = world_hash_;
  mixWorld("transaction.commit", record->info.transaction_id);
  mixWorld("transaction.label", record->label);
  mixWorld("transaction.provenance", record->provenance);
  for (const WorldComponentAccess &access : record->accesses) {
    mixWorld("transaction.access", accessStamp(access));
    committed_accesses_this_tick_.push_back(access);
  }
  record->active = false;
  record->info.committed = true;
  record->info.post_world_hash = world_hash_;
  record->info.access_count = record->accesses.size();
  appendEvent({.kind = WorldTraceEventKind::SchedulerDecision,
               .tick = current_tick_,
               .transaction_id = record->info.transaction_id,
               .label = record->label,
               .detail = "accepted",
               .parent_world_hash = record->info.parent_world_hash,
               .world_hash = world_hash_});
  appendEvent({.kind = WorldTraceEventKind::TransactionCommit,
               .tick = current_tick_,
               .transaction_id = record->info.transaction_id,
               .label = record->label,
               .detail = record->provenance,
               .parent_world_hash = record->info.parent_world_hash,
               .world_hash = world_hash_});
  return record->info;
}

WorldTransactionInfo WorldState::abortTransaction(const std::uint64_t transaction_id,
                                                  std::string reason) {
  TransactionRecord *record = transactionRecord(transaction_id);
  if (record == nullptr || !record->active) {
    appendValidation("transaction.abort", "transaction is not active");
    return {.transaction_id = transaction_id, .diagnostic = "transaction is not active"};
  }
  record->active = false;
  record->info.committed = false;
  record->info.diagnostic = reason;
  appendEvent({.kind = WorldTraceEventKind::TransactionAbort,
               .tick = current_tick_,
               .transaction_id = record->info.transaction_id,
               .label = record->label,
               .detail = std::move(reason),
               .parent_world_hash = record->info.parent_world_hash,
               .world_hash = world_hash_});
  return record->info;
}

WorldTraceCounts WorldState::counts() const {
  const std::size_t live = std::count_if(entities_.begin(), entities_.end(),
                                         [](const EntityRecord &record) { return record.alive; });
  const std::size_t validation = std::count_if(
      trace_events_.begin(), trace_events_.end(), [](const WorldTraceEvent &event) {
        return event.kind == WorldTraceEventKind::ValidationError;
      });
  return {.event_count = trace_events_.size(),
          .entity_count = entities_.size(),
          .live_entity_count = live,
          .transaction_count = transactions_.size(),
          .validation_event_count = validation,
          .tick = current_tick_,
          .time_seconds = time_seconds_,
          .world_hash = world_hash_,
          .trace_hash = trace_hash_};
}

const std::vector<WorldTraceEvent> &WorldState::traceEvents() const noexcept {
  return trace_events_;
}

std::vector<WorldTraceEvent> WorldState::validationEvents() const {
  std::vector<WorldTraceEvent> out;
  for (const WorldTraceEvent &event : trace_events_) {
    if (event.kind == WorldTraceEventKind::ValidationError) {
      out.push_back(event);
    }
  }
  return out;
}

bool WorldState::saveSnapshot(const std::filesystem::path &path, std::string *diagnostic) {
  std::ofstream output(path);
  if (!output.good()) {
    if (diagnostic != nullptr) {
      *diagnostic = "could not open snapshot for writing";
    }
    appendValidation("snapshot.save", "could not open snapshot for writing");
    return false;
  }
  appendEvent({.kind = WorldTraceEventKind::SnapshotSaved,
               .tick = current_tick_,
               .label = "snapshot",
               .detail = path.string(),
               .world_hash = world_hash_});
  output << "aster_world_snapshot_v1\n";
  output << "schema_version=" << kWorldSnapshotSchemaVersion << "\n";
  output << "tick=" << current_tick_ << "\n";
  output << "time_seconds=" << std::setprecision(17) << time_seconds_ << "\n";
  output << "world_hash=" << world_hash_ << "\n";
  output << "trace_hash=" << trace_hash_ << "\n";
  output << "next_entity_id=" << next_entity_id_ << "\n";
  for (const EntityRecord &entity : entities_) {
    output << "entity=" << entity.handle.id << "|" << entity.handle.generation << "|"
           << (entity.alive ? 1 : 0) << "|" << entity.label << "\n";
  }
  return true;
}

WorldMigrationReport WorldState::loadSnapshot(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input.good()) {
    appendValidation("snapshot.load", "could not open snapshot for reading");
    return {.diagnostic = "could not open snapshot for reading"};
  }

  std::string line;
  std::getline(input, line);
  if (line != "aster_world_snapshot_v1") {
    appendValidation("snapshot.load", "snapshot header is invalid");
    return {.diagnostic = "snapshot header is invalid"};
  }

  std::unordered_map<std::string, std::string> fields;
  std::vector<EntityRecord> loaded_entities;
  while (std::getline(input, line)) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0u, equals);
    const std::string value = line.substr(equals + 1u);
    if (key == "entity") {
      const std::vector<std::string> parts = split(value, '|');
      if (parts.size() >= 4u) {
        EntityRecord record;
        (void)parseInteger(parts[0], record.handle.id);
        (void)parseInteger(parts[1], record.handle.generation);
        int alive = 0;
        (void)parseInteger(parts[2], alive);
        record.alive = alive != 0;
        record.label = parts[3];
        loaded_entities.push_back(std::move(record));
      }
    } else {
      fields[key] = value;
    }
  }

  std::uint32_t schema_version = 0u;
  (void)parseInteger(fields["schema_version"], schema_version);
  (void)parseInteger(fields["tick"], current_tick_);
  (void)parseInteger(fields["world_hash"], world_hash_);
  (void)parseInteger(fields["trace_hash"], trace_hash_);
  (void)parseInteger(fields["next_entity_id"], next_entity_id_);
  if (auto found = fields.find("time_seconds"); found != fields.end()) {
    try {
      time_seconds_ = std::stod(found->second);
    } catch (...) {
      appendValidation("snapshot.load", "snapshot time value is invalid");
      return {.diagnostic = "snapshot time value is invalid"};
    }
  }
  entities_ = std::move(loaded_entities);
  transactions_.clear();
  committed_accesses_this_tick_.clear();
  appendEvent({.kind = WorldTraceEventKind::SnapshotLoaded,
               .tick = current_tick_,
               .label = "snapshot",
               .detail = path.string(),
               .world_hash = world_hash_});
  if (schema_version != kWorldSnapshotSchemaVersion) {
    appendEvent({.kind = WorldTraceEventKind::Migration,
                 .tick = current_tick_,
                 .label = "snapshot-migration",
                 .detail = "schema " + std::to_string(schema_version) + " -> " +
                           std::to_string(kWorldSnapshotSchemaVersion),
                 .world_hash = world_hash_});
  }
  return {.loaded_schema_version = schema_version,
          .current_schema_version = kWorldSnapshotSchemaVersion,
          .migration_applied = schema_version != kWorldSnapshotSchemaVersion,
          .entity_count = entities_.size(),
          .world_hash = world_hash_,
          .trace_hash = trace_hash_};
}

WorldReplayReport WorldState::replaySnapshot(const std::filesystem::path &path,
                                             const std::uint64_t expected_world_hash) {
  WorldState replay(config_);
  WorldMigrationReport migration = replay.loadSnapshot(path);
  const bool matched = migration.world_hash != 0u &&
                       (expected_world_hash == 0u ||
                        migration.world_hash == expected_world_hash);
  appendEvent({.kind = WorldTraceEventKind::Replay,
               .tick = current_tick_,
               .label = "snapshot-replay",
               .detail = matched ? "matched" : "world hash mismatch",
               .world_hash = world_hash_});
  if (!matched) {
    appendValidation("snapshot.replay", "replay world hash mismatch");
  }
  return {.matched = matched,
          .events_replayed = replay.traceEvents().size(),
          .expected_world_hash = expected_world_hash,
          .actual_world_hash = migration.world_hash,
          .actual_trace_hash = migration.trace_hash,
          .diagnostic = matched ? std::string() : "replay world hash mismatch"};
}

std::vector<ResidencyDecision>
WorldState::planResidency(const ResidencyBudget budget, const std::vector<ResidencyAsset> &assets) {
  std::vector<std::size_t> order(assets.size());
  for (std::size_t index = 0u; index < order.size(); ++index) {
    order[index] = index;
  }
  std::sort(order.begin(), order.end(), [&](const std::size_t lhs_index,
                                            const std::size_t rhs_index) {
    const ResidencyAsset &lhs = assets[lhs_index];
    const ResidencyAsset &rhs = assets[rhs_index];
    if (lhs.visible != rhs.visible) {
      return lhs.visible && !rhs.visible;
    }
    if (lhs.priority != rhs.priority) {
      return lhs.priority > rhs.priority;
    }
    return lhs.asset_id < rhs.asset_id;
  });

  std::uint64_t used = 0u;
  std::vector<bool> selected(assets.size(), false);
  for (const std::size_t index : order) {
    const ResidencyAsset &asset = assets[index];
    if (!asset.visible || asset.byte_cost == 0u) {
      continue;
    }
    if (used + asset.byte_cost <= budget.byte_budget) {
      used += asset.byte_cost;
      selected[index] = true;
    }
  }

  std::vector<ResidencyDecision> decisions;
  decisions.reserve(assets.size());
  for (std::size_t index = 0u; index < assets.size(); ++index) {
    const ResidencyAsset &asset = assets[index];
    ResidencyDecision decision;
    decision.asset_id = asset.asset_id;
    decision.byte_cost = asset.byte_cost;
    decision.priority = asset.priority;
    decision.visible = asset.visible;
    if (selected[index] && asset.resident) {
      decision.decision = ResidencyDecisionKind::Keep;
      decision.reason = "visible resident within budget";
    } else if (selected[index]) {
      decision.decision = ResidencyDecisionKind::Load;
      decision.reason = "visible candidate within budget";
    } else if (asset.resident) {
      decision.decision = ResidencyDecisionKind::Evict;
      decision.reason = asset.visible ? "visible candidate exceeded budget" : "not visible";
    } else {
      decision.decision = ResidencyDecisionKind::Reject;
      decision.reason = asset.visible ? "visible candidate exceeded budget" : "not visible";
    }
    decisions.push_back(std::move(decision));
  }

  for (const ResidencyDecision &decision : decisions) {
    mixWorld("residency", decision.asset_id);
    mixWorld("residency.decision", static_cast<std::uint64_t>(decision.decision));
    appendEvent({.kind = WorldTraceEventKind::ResidencyDecision,
                 .tick = current_tick_,
                 .label = decision.asset_id,
                 .detail = decision.reason,
                 .world_hash = world_hash_});
  }
  return decisions;
}

std::uint64_t WorldState::currentTick() const noexcept {
  return current_tick_;
}

double WorldState::timeSeconds() const noexcept {
  return time_seconds_;
}

std::uint64_t WorldState::worldHash() const noexcept {
  return world_hash_;
}

std::uint64_t WorldState::traceHash() const noexcept {
  return trace_hash_;
}

WorldState::EntityRecord *WorldState::entityRecord(const WorldEntityHandle handle) {
  const auto found = std::find_if(entities_.begin(), entities_.end(), [&](EntityRecord &record) {
    return record.handle.id == handle.id && record.handle.generation == handle.generation;
  });
  return found == entities_.end() ? nullptr : &*found;
}

const WorldState::EntityRecord *WorldState::entityRecord(const WorldEntityHandle handle) const {
  const auto found = std::find_if(entities_.begin(), entities_.end(),
                                  [&](const EntityRecord &record) {
                                    return record.handle.id == handle.id &&
                                           record.handle.generation == handle.generation;
                                  });
  return found == entities_.end() ? nullptr : &*found;
}

WorldState::TransactionRecord *WorldState::transactionRecord(const std::uint64_t transaction_id) {
  const auto found = std::find_if(transactions_.begin(), transactions_.end(),
                                  [&](TransactionRecord &record) {
                                    return record.info.transaction_id == transaction_id;
                                  });
  return found == transactions_.end() ? nullptr : &*found;
}

const WorldState::TransactionRecord *
WorldState::transactionRecord(const std::uint64_t transaction_id) const {
  const auto found = std::find_if(transactions_.begin(), transactions_.end(),
                                  [&](const TransactionRecord &record) {
                                    return record.info.transaction_id == transaction_id;
                                  });
  return found == transactions_.end() ? nullptr : &*found;
}

bool WorldState::hasAccessHazard(const WorldComponentAccess &access,
                                 WorldComponentAccess *conflict) const {
  const auto found = std::find_if(
      committed_accesses_this_tick_.begin(), committed_accesses_this_tick_.end(),
      [&](const WorldComponentAccess &prior) { return accessConflicts(access, prior); });
  if (found == committed_accesses_this_tick_.end()) {
    return false;
  }
  if (conflict != nullptr) {
    *conflict = *found;
  }
  return true;
}

void WorldState::appendEvent(WorldTraceEvent event) {
  event.sequence = next_event_sequence_++;
  if (event.tick == 0u) {
    event.tick = current_tick_;
  }
  if (event.world_hash == 0u) {
    event.world_hash = world_hash_;
  }
  trace_hash_ = fnvAppend(trace_hash_, static_cast<std::uint64_t>(event.kind));
  trace_hash_ = fnvAppend(trace_hash_, event.sequence);
  trace_hash_ = fnvAppend(trace_hash_, event.tick);
  trace_hash_ = fnvAppend(trace_hash_, event.transaction_id);
  trace_hash_ = fnvAppend(trace_hash_, event.entity.id);
  trace_hash_ = fnvAppend(trace_hash_, event.entity.generation);
  trace_hash_ = fnvAppend(trace_hash_, event.label);
  trace_hash_ = fnvAppend(trace_hash_, event.subject);
  trace_hash_ = fnvAppend(trace_hash_, event.component);
  trace_hash_ = fnvAppend(trace_hash_, event.detail);
  trace_hash_ = fnvAppend(trace_hash_, event.world_hash);
  event.trace_hash = trace_hash_;
  trace_events_.push_back(std::move(event));
}

void WorldState::appendValidation(std::string label, std::string detail) {
  appendEvent({.kind = WorldTraceEventKind::ValidationError,
               .tick = current_tick_,
               .label = std::move(label),
               .detail = std::move(detail),
               .world_hash = world_hash_});
}

void WorldState::mixWorld(const std::string_view tag, const std::uint64_t value) {
  world_hash_ = fnvAppend(world_hash_, tag);
  world_hash_ = fnvAppend(world_hash_, value);
}

void WorldState::mixWorld(const std::string_view tag, const std::string_view value) {
  world_hash_ = fnvAppend(world_hash_, tag);
  world_hash_ = fnvAppend(world_hash_, value);
}

const char *worldComponentAccessModeName(const WorldComponentAccessMode mode) {
  switch (mode) {
  case WorldComponentAccessMode::Write:
    return "write";
  case WorldComponentAccessMode::Read:
  default:
    return "read";
  }
}

const char *worldTraceEventKindName(const WorldTraceEventKind kind) {
  switch (kind) {
  case WorldTraceEventKind::InputEvent:
    return "input-event";
  case WorldTraceEventKind::SimulationTick:
    return "simulation-tick";
  case WorldTraceEventKind::SchedulerDecision:
    return "scheduler-decision";
  case WorldTraceEventKind::AssetResolution:
    return "asset-resolution";
  case WorldTraceEventKind::ResidencyDecision:
    return "residency-decision";
  case WorldTraceEventKind::RenderableExtraction:
    return "renderable-extraction";
  case WorldTraceEventKind::FrameSubmission:
    return "frame-submission";
  case WorldTraceEventKind::TransactionBegin:
    return "transaction-begin";
  case WorldTraceEventKind::TransactionCommit:
    return "transaction-commit";
  case WorldTraceEventKind::TransactionAbort:
    return "transaction-abort";
  case WorldTraceEventKind::EntityCreated:
    return "entity-created";
  case WorldTraceEventKind::EntityDestroyed:
    return "entity-destroyed";
  case WorldTraceEventKind::SnapshotSaved:
    return "snapshot-saved";
  case WorldTraceEventKind::SnapshotLoaded:
    return "snapshot-loaded";
  case WorldTraceEventKind::Migration:
    return "migration";
  case WorldTraceEventKind::Replay:
    return "replay";
  case WorldTraceEventKind::ValidationError:
  default:
    return "validation-error";
  }
}

const char *residencyDecisionKindName(const ResidencyDecisionKind kind) {
  switch (kind) {
  case ResidencyDecisionKind::Keep:
    return "keep";
  case ResidencyDecisionKind::Load:
    return "load";
  case ResidencyDecisionKind::Evict:
    return "evict";
  case ResidencyDecisionKind::Reject:
  default:
    return "reject";
  }
}

} // namespace aster
