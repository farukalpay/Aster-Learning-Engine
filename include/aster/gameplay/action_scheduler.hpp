// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/gameplay/feature_labels.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

using ActionTaskId = std::uint64_t;

enum class ActionTaskPriority : std::uint32_t {
  Background = 0u,
  Normal = 1u,
  High = 2u,
  Critical = 3u,
};

enum class ActionTaskState : std::uint32_t {
  Waiting,
  Blocked,
  Active,
  Finished,
  Cancelled,
};

enum class ActionTaskEventKind : std::uint32_t {
  Submitted,
  Activated,
  Ticked,
  Finished,
  Cancelled,
  Blocked,
};

class ActionLaneSet {
public:
  bool add(FeatureLabel lane);
  bool add(std::string_view lane);
  bool remove(const FeatureLabel &lane);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool contains(const FeatureLabel &lane) const;
  [[nodiscard]] bool overlaps(const ActionLaneSet &other) const;
  [[nodiscard]] std::vector<FeatureLabel> lanes() const;
  [[nodiscard]] std::uint64_t stamp() const;
  [[nodiscard]] std::string debugString() const;

private:
  std::vector<FeatureLabel> lanes_;
};

class ActionResourceSet {
public:
  bool add(FeatureLabel resource);
  bool add(std::string_view resource);
  bool remove(const FeatureLabel &resource);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool contains(const FeatureLabel &resource) const;
  [[nodiscard]] bool overlaps(const ActionResourceSet &other) const;
  [[nodiscard]] std::vector<FeatureLabel> resources() const;
  [[nodiscard]] std::uint64_t stamp() const;
  [[nodiscard]] std::string debugString() const;

private:
  std::vector<FeatureLabel> resources_;
};

struct ActionTaskContext {
  ActionTaskId id = 0u;
  std::string_view name;
  std::string_view owner;
  float elapsed_seconds = 0.0f;
  float dt = 0.0f;
  FeatureLabelSet world_labels;
  FeatureLabelSet task_labels;
  ActionResourceSet required_resources;
  ActionResourceSet claimed_resources;
};

using ActionTaskFn = std::function<void(ActionTaskContext &)>;

struct ActionTaskDesc {
  std::string name;
  std::string owner;
  ActionTaskPriority priority = ActionTaskPriority::Normal;
  ActionLaneSet lanes;
  ActionResourceSet required_resources;
  ActionResourceSet claimed_resources;
  FeatureLabelSet labels;
  FeatureLabelQuery requirements;
  float duration_seconds = 0.0f;
  bool can_preempt = false;
  ActionTaskFn on_start;
  ActionTaskFn on_tick;
  ActionTaskFn on_finish;
};

struct ActionTaskRecord {
  ActionTaskId id = 0u;
  std::string name;
  std::string owner;
  ActionTaskPriority priority = ActionTaskPriority::Normal;
  ActionTaskState state = ActionTaskState::Waiting;
  ActionLaneSet lanes;
  ActionResourceSet required_resources;
  ActionResourceSet claimed_resources;
  FeatureLabelSet labels;
  float elapsed_seconds = 0.0f;
  float duration_seconds = 0.0f;
  std::uint64_t sequence = 0u;
  std::uint64_t deterministic_stamp = 0u;
  std::string diagnostic;
  std::string owner_diagnostic;
  std::vector<ActionTaskId> blocked_by_tasks;
  bool can_preempt = false;
};

struct ActionTaskEvent {
  ActionTaskEventKind kind = ActionTaskEventKind::Submitted;
  ActionTaskId task_id = 0u;
  std::string task_name;
  std::string owner;
  ActionTaskState state = ActionTaskState::Waiting;
  float time_seconds = 0.0f;
  std::uint64_t sequence = 0u;
  std::uint64_t lane_stamp = 0u;
  std::uint64_t resource_stamp = 0u;
  std::string detail;
};

struct ActionTaskJournalEntry {
  ActionTaskEventKind kind = ActionTaskEventKind::Submitted;
  ActionTaskId task_id = 0u;
  std::string task_name;
  std::string owner;
  ActionTaskState state = ActionTaskState::Waiting;
  float time_seconds = 0.0f;
  std::uint64_t sequence = 0u;
  std::uint64_t deterministic_stamp = 0u;
  std::uint64_t lane_stamp = 0u;
  std::uint64_t resource_stamp = 0u;
  std::string detail;
};

class ActionTaskJournal {
public:
  void record(ActionTaskJournalEntry entry);
  void clear();

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const std::vector<ActionTaskJournalEntry> &entries() const noexcept;
  [[nodiscard]] std::vector<ActionTaskJournalEntry> entriesFor(ActionTaskId task_id) const;
  [[nodiscard]] std::uint64_t contractStamp() const;
  [[nodiscard]] std::string summary() const;

private:
  std::vector<ActionTaskJournalEntry> entries_;
};

struct ActionOwnerDiagnostic {
  std::string owner;
  std::size_t waiting_tasks = 0u;
  std::size_t blocked_tasks = 0u;
  std::size_t active_tasks = 0u;
  std::size_t finished_tasks = 0u;
  std::size_t cancelled_tasks = 0u;
  std::uint64_t lane_stamp = 0u;
  std::uint64_t resource_stamp = 0u;
  std::vector<std::string> diagnostics;
};

struct ActionSchedulerFrame {
  float time_seconds = 0.0f;
  std::vector<ActionTaskEvent> events;
  std::size_t active_tasks = 0u;
  std::size_t waiting_tasks = 0u;
  std::size_t finished_tasks = 0u;
  std::size_t cancelled_tasks = 0u;
};

class ActionScheduler {
public:
  ActionTaskId submit(ActionTaskDesc desc);
  bool cancel(ActionTaskId id, std::string reason = {});
  ActionSchedulerFrame tick(float dt);
  void clear();
  void beginEventBatch();
  void endEventBatch();

  void setWorldLabels(FeatureLabelSet labels);
  [[nodiscard]] const FeatureLabelSet &worldLabels() const;

  [[nodiscard]] const ActionTaskRecord *find(ActionTaskId id) const;
  [[nodiscard]] std::vector<ActionTaskRecord> records() const;
  [[nodiscard]] std::vector<ActionTaskEvent> events() const;
  [[nodiscard]] const ActionTaskJournal &journal() const noexcept;
  [[nodiscard]] ActionLaneSet activeLanes() const;
  [[nodiscard]] ActionResourceSet claimedResources() const;
  [[nodiscard]] std::vector<ActionOwnerDiagnostic> ownerDiagnostics() const;

private:
  struct ActionTaskNode {
    ActionTaskRecord record;
    FeatureLabelQuery requirements;
    ActionTaskFn on_start;
    ActionTaskFn on_tick;
    ActionTaskFn on_finish;
  };

  [[nodiscard]] std::optional<std::size_t> indexOf(ActionTaskId id) const;
  [[nodiscard]] std::vector<std::size_t> conflictingActiveTasks(const ActionTaskRecord &record) const;
  [[nodiscard]] ActionTaskContext makeContext(const ActionTaskNode &node, float dt) const;
  [[nodiscard]] ActionResourceSet resourceFootprint(const ActionTaskRecord &record) const;
  [[nodiscard]] std::uint64_t deterministicStamp(const ActionTaskRecord &record) const;
  void activateTask(std::size_t index);
  void finishTask(std::size_t index, ActionTaskState state, std::string reason);
  void emit(ActionTaskEventKind kind, const ActionTaskNode &node, std::string detail = {});
  void flushDeferredEvents();

  std::vector<ActionTaskNode> tasks_;
  std::vector<ActionTaskEvent> events_;
  std::vector<ActionTaskEvent> deferred_events_;
  ActionTaskJournal journal_;
  FeatureLabelSet world_labels_;
  ActionTaskId next_id_ = 1u;
  std::uint64_t next_sequence_ = 1u;
  std::uint64_t next_event_sequence_ = 1u;
  std::uint32_t event_batch_depth_ = 0u;
  float time_seconds_ = 0.0f;
};

[[nodiscard]] const char *actionTaskPriorityName(ActionTaskPriority priority);
[[nodiscard]] const char *actionTaskStateName(ActionTaskState state);
[[nodiscard]] const char *actionTaskEventKindName(ActionTaskEventKind kind);

} // namespace aster
