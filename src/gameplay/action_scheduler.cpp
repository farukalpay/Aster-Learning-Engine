// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/gameplay/action_scheduler.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aster {

namespace {

void sortUniqueLanes(std::vector<FeatureLabel> &lanes) {
  std::sort(lanes.begin(), lanes.end());
  lanes.erase(std::unique(lanes.begin(), lanes.end()), lanes.end());
}

bool isTerminal(const ActionTaskState state) {
  return state == ActionTaskState::Finished || state == ActionTaskState::Cancelled;
}

std::uint32_t priorityValue(const ActionTaskPriority priority) {
  return static_cast<std::uint32_t>(priority);
}

} // namespace

bool ActionLaneSet::add(FeatureLabel lane) {
  if (!lane.valid() || contains(lane)) {
    return false;
  }
  lanes_.push_back(std::move(lane));
  sortUniqueLanes(lanes_);
  return true;
}

bool ActionLaneSet::add(const std::string_view lane) {
  const std::optional<FeatureLabel> parsed = parseFeatureLabel(lane);
  return parsed ? add(*parsed) : false;
}

bool ActionLaneSet::remove(const FeatureLabel &lane) {
  const auto found = std::find(lanes_.begin(), lanes_.end(), lane);
  if (found == lanes_.end()) {
    return false;
  }
  lanes_.erase(found);
  return true;
}

void ActionLaneSet::clear() {
  lanes_.clear();
}

bool ActionLaneSet::empty() const noexcept {
  return lanes_.empty();
}

std::size_t ActionLaneSet::size() const noexcept {
  return lanes_.size();
}

bool ActionLaneSet::contains(const FeatureLabel &lane) const {
  return std::find(lanes_.begin(), lanes_.end(), lane) != lanes_.end();
}

bool ActionLaneSet::overlaps(const ActionLaneSet &other) const {
  for (const FeatureLabel &lane : lanes_) {
    if (other.contains(lane)) {
      return true;
    }
  }
  return false;
}

std::vector<FeatureLabel> ActionLaneSet::lanes() const {
  return lanes_;
}

std::uint64_t ActionLaneSet::stamp() const {
  std::uint64_t result = 0xA57EA110CAFE0001ull;
  for (const FeatureLabel &lane : lanes_) {
    result = hashCombine64(result, lane.stableId());
  }
  return result;
}

std::string ActionLaneSet::debugString() const {
  std::ostringstream out;
  for (std::size_t i = 0u; i < lanes_.size(); ++i) {
    if (i > 0u) {
      out << ",";
    }
    out << lanes_[i].path();
  }
  return out.str();
}

ActionTaskId ActionScheduler::submit(ActionTaskDesc desc) {
  const ActionTaskId id = next_id_++;
  if (desc.name.empty()) {
    desc.name = "action." + std::to_string(id);
  }

  ActionTaskNode node;
  node.record.id = id;
  node.record.name = std::move(desc.name);
  node.record.owner = std::move(desc.owner);
  node.record.priority = desc.priority;
  node.record.lanes = std::move(desc.lanes);
  node.record.labels = std::move(desc.labels);
  node.record.duration_seconds = desc.duration_seconds;
  node.record.sequence = next_sequence_++;
  node.record.can_preempt = desc.can_preempt;
  node.requirements = std::move(desc.requirements);
  node.on_start = std::move(desc.on_start);
  node.on_tick = std::move(desc.on_tick);
  node.on_finish = std::move(desc.on_finish);
  tasks_.push_back(std::move(node));
  emit(ActionTaskEventKind::Submitted, tasks_.back());
  return id;
}

bool ActionScheduler::cancel(const ActionTaskId id, std::string reason) {
  const std::optional<std::size_t> index = indexOf(id);
  if (!index || isTerminal(tasks_[*index].record.state)) {
    return false;
  }
  finishTask(*index, ActionTaskState::Cancelled,
             reason.empty() ? std::string("cancelled") : std::move(reason));
  return true;
}

ActionSchedulerFrame ActionScheduler::tick(const float dt) {
  time_seconds_ += std::max(0.0f, dt);
  const std::size_t first_event = events_.size();

  std::vector<std::size_t> candidates;
  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    const ActionTaskState state = tasks_[index].record.state;
    if (state == ActionTaskState::Waiting || state == ActionTaskState::Blocked) {
      candidates.push_back(index);
    }
  }

  std::sort(candidates.begin(), candidates.end(), [&](const std::size_t lhs, const std::size_t rhs) {
    const ActionTaskRecord &a = tasks_[lhs].record;
    const ActionTaskRecord &b = tasks_[rhs].record;
    if (a.priority != b.priority) {
      return priorityValue(a.priority) > priorityValue(b.priority);
    }
    return a.sequence < b.sequence;
  });

  for (const std::size_t index : candidates) {
    ActionTaskNode &task = tasks_[index];
    if (!task.requirements.matches(world_labels_)) {
      if (task.record.state != ActionTaskState::Blocked) {
        task.record.state = ActionTaskState::Blocked;
        std::vector<std::string> diagnostics = task.requirements.explainMismatch(world_labels_);
        task.record.diagnostic = diagnostics.empty() ? "requirements not met" : diagnostics.front();
        emit(ActionTaskEventKind::Blocked, task, task.record.diagnostic);
      }
      continue;
    }

    std::vector<std::size_t> conflicts = conflictingActiveTasks(task.record.lanes);
    if (!conflicts.empty() && task.record.can_preempt) {
      bool can_preempt_all = true;
      for (const std::size_t conflict : conflicts) {
        can_preempt_all = can_preempt_all &&
                          priorityValue(task.record.priority) >
                              priorityValue(tasks_[conflict].record.priority);
      }
      if (can_preempt_all) {
        for (const std::size_t conflict : conflicts) {
          finishTask(conflict, ActionTaskState::Cancelled,
                     "preempted by " + task.record.name);
        }
        conflicts = conflictingActiveTasks(task.record.lanes);
      }
    }
    if (conflicts.empty()) {
      activateTask(index);
    } else if (task.record.state == ActionTaskState::Blocked) {
      task.record.state = ActionTaskState::Waiting;
      task.record.diagnostic.clear();
    }
  }

  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    ActionTaskNode &task = tasks_[index];
    if (task.record.state != ActionTaskState::Active) {
      continue;
    }
    ActionTaskContext context = makeContext(task, dt);
    if (task.on_tick) {
      task.on_tick(context);
    }
    task.record.elapsed_seconds += std::max(0.0f, dt);
    emit(ActionTaskEventKind::Ticked, task);
    if (task.record.duration_seconds > 0.0f &&
        task.record.elapsed_seconds + 0.00001f >= task.record.duration_seconds) {
      finishTask(index, ActionTaskState::Finished, "duration elapsed");
    }
  }

  ActionSchedulerFrame frame;
  frame.time_seconds = time_seconds_;
  frame.events.assign(events_.begin() + static_cast<std::ptrdiff_t>(first_event), events_.end());
  for (const ActionTaskNode &task : tasks_) {
    switch (task.record.state) {
    case ActionTaskState::Waiting:
    case ActionTaskState::Blocked:
      ++frame.waiting_tasks;
      break;
    case ActionTaskState::Active:
      ++frame.active_tasks;
      break;
    case ActionTaskState::Finished:
      ++frame.finished_tasks;
      break;
    case ActionTaskState::Cancelled:
      ++frame.cancelled_tasks;
      break;
    }
  }
  return frame;
}

void ActionScheduler::clear() {
  tasks_.clear();
  events_.clear();
  world_labels_.clear();
  next_id_ = 1u;
  next_sequence_ = 1u;
  time_seconds_ = 0.0f;
}

void ActionScheduler::setWorldLabels(FeatureLabelSet labels) {
  world_labels_ = std::move(labels);
}

const FeatureLabelSet &ActionScheduler::worldLabels() const {
  return world_labels_;
}

const ActionTaskRecord *ActionScheduler::find(const ActionTaskId id) const {
  const std::optional<std::size_t> index = indexOf(id);
  return index ? &tasks_[*index].record : nullptr;
}

std::vector<ActionTaskRecord> ActionScheduler::records() const {
  std::vector<ActionTaskRecord> result;
  result.reserve(tasks_.size());
  for (const ActionTaskNode &task : tasks_) {
    result.push_back(task.record);
  }
  return result;
}

std::vector<ActionTaskEvent> ActionScheduler::events() const {
  return events_;
}

ActionLaneSet ActionScheduler::activeLanes() const {
  ActionLaneSet lanes;
  for (const ActionTaskNode &task : tasks_) {
    if (task.record.state != ActionTaskState::Active) {
      continue;
    }
    for (const FeatureLabel &lane : task.record.lanes.lanes()) {
      lanes.add(lane);
    }
  }
  return lanes;
}

std::optional<std::size_t> ActionScheduler::indexOf(const ActionTaskId id) const {
  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    if (tasks_[index].record.id == id) {
      return index;
    }
  }
  return std::nullopt;
}

std::vector<std::size_t> ActionScheduler::conflictingActiveTasks(const ActionLaneSet &lanes) const {
  std::vector<std::size_t> conflicts;
  if (lanes.empty()) {
    return conflicts;
  }
  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    if (tasks_[index].record.state == ActionTaskState::Active &&
        tasks_[index].record.lanes.overlaps(lanes)) {
      conflicts.push_back(index);
    }
  }
  return conflicts;
}

ActionTaskContext ActionScheduler::makeContext(const ActionTaskNode &node, const float dt) const {
  return ActionTaskContext{node.record.id,
                           node.record.name,
                           node.record.owner,
                           node.record.elapsed_seconds,
                           dt,
                           world_labels_,
                           node.record.labels};
}

void ActionScheduler::activateTask(const std::size_t index) {
  ActionTaskNode &task = tasks_[index];
  if (task.record.state == ActionTaskState::Active || isTerminal(task.record.state)) {
    return;
  }
  task.record.state = ActionTaskState::Active;
  task.record.diagnostic.clear();
  ActionTaskContext context = makeContext(task, 0.0f);
  if (task.on_start) {
    task.on_start(context);
  }
  emit(ActionTaskEventKind::Activated, task);
}

void ActionScheduler::finishTask(const std::size_t index, const ActionTaskState state,
                                 std::string reason) {
  ActionTaskNode &task = tasks_[index];
  if (isTerminal(task.record.state)) {
    return;
  }
  task.record.state = state;
  task.record.diagnostic = reason;
  ActionTaskContext context = makeContext(task, 0.0f);
  if (task.on_finish) {
    task.on_finish(context);
  }
  emit(state == ActionTaskState::Cancelled ? ActionTaskEventKind::Cancelled
                                           : ActionTaskEventKind::Finished,
       task, std::move(reason));
}

void ActionScheduler::emit(const ActionTaskEventKind kind, const ActionTaskNode &node,
                           std::string detail) {
  events_.push_back({kind, node.record.id, node.record.name, node.record.state, time_seconds_,
                     std::move(detail)});
}

const char *actionTaskPriorityName(const ActionTaskPriority priority) {
  switch (priority) {
  case ActionTaskPriority::Background:
    return "background";
  case ActionTaskPriority::Normal:
    return "normal";
  case ActionTaskPriority::High:
    return "high";
  case ActionTaskPriority::Critical:
    return "critical";
  }
  return "unknown";
}

const char *actionTaskStateName(const ActionTaskState state) {
  switch (state) {
  case ActionTaskState::Waiting:
    return "waiting";
  case ActionTaskState::Blocked:
    return "blocked";
  case ActionTaskState::Active:
    return "active";
  case ActionTaskState::Finished:
    return "finished";
  case ActionTaskState::Cancelled:
    return "cancelled";
  }
  return "unknown";
}

const char *actionTaskEventKindName(const ActionTaskEventKind kind) {
  switch (kind) {
  case ActionTaskEventKind::Submitted:
    return "submitted";
  case ActionTaskEventKind::Activated:
    return "activated";
  case ActionTaskEventKind::Ticked:
    return "ticked";
  case ActionTaskEventKind::Finished:
    return "finished";
  case ActionTaskEventKind::Cancelled:
    return "cancelled";
  case ActionTaskEventKind::Blocked:
    return "blocked";
  }
  return "unknown";
}

} // namespace aster
