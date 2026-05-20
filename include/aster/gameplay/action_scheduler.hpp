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

struct ActionTaskContext {
  ActionTaskId id = 0u;
  std::string_view name;
  std::string_view owner;
  float elapsed_seconds = 0.0f;
  float dt = 0.0f;
  FeatureLabelSet world_labels;
  FeatureLabelSet task_labels;
};

using ActionTaskFn = std::function<void(ActionTaskContext &)>;

struct ActionTaskDesc {
  std::string name;
  std::string owner;
  ActionTaskPriority priority = ActionTaskPriority::Normal;
  ActionLaneSet lanes;
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
  FeatureLabelSet labels;
  float elapsed_seconds = 0.0f;
  float duration_seconds = 0.0f;
  std::uint64_t sequence = 0u;
  std::string diagnostic;
  bool can_preempt = false;
};

struct ActionTaskEvent {
  ActionTaskEventKind kind = ActionTaskEventKind::Submitted;
  ActionTaskId task_id = 0u;
  std::string task_name;
  ActionTaskState state = ActionTaskState::Waiting;
  float time_seconds = 0.0f;
  std::string detail;
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

  void setWorldLabels(FeatureLabelSet labels);
  [[nodiscard]] const FeatureLabelSet &worldLabels() const;

  [[nodiscard]] const ActionTaskRecord *find(ActionTaskId id) const;
  [[nodiscard]] std::vector<ActionTaskRecord> records() const;
  [[nodiscard]] std::vector<ActionTaskEvent> events() const;
  [[nodiscard]] ActionLaneSet activeLanes() const;

private:
  struct ActionTaskNode {
    ActionTaskRecord record;
    FeatureLabelQuery requirements;
    ActionTaskFn on_start;
    ActionTaskFn on_tick;
    ActionTaskFn on_finish;
  };

  [[nodiscard]] std::optional<std::size_t> indexOf(ActionTaskId id) const;
  [[nodiscard]] std::vector<std::size_t> conflictingActiveTasks(const ActionLaneSet &lanes) const;
  [[nodiscard]] ActionTaskContext makeContext(const ActionTaskNode &node, float dt) const;
  void activateTask(std::size_t index);
  void finishTask(std::size_t index, ActionTaskState state, std::string reason);
  void emit(ActionTaskEventKind kind, const ActionTaskNode &node, std::string detail = {});

  std::vector<ActionTaskNode> tasks_;
  std::vector<ActionTaskEvent> events_;
  FeatureLabelSet world_labels_;
  ActionTaskId next_id_ = 1u;
  std::uint64_t next_sequence_ = 1u;
  float time_seconds_ = 0.0f;
};

[[nodiscard]] const char *actionTaskPriorityName(ActionTaskPriority priority);
[[nodiscard]] const char *actionTaskStateName(ActionTaskState state);
[[nodiscard]] const char *actionTaskEventKindName(ActionTaskEventKind kind);

} // namespace aster
