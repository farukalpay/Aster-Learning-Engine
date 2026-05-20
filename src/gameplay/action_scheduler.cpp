// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/gameplay/action_scheduler.hpp"

#include "aster/math/hash.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>

namespace aster {

namespace {

void sortUniqueLanes(std::vector<FeatureLabel> &lanes) {
  std::sort(lanes.begin(), lanes.end());
  lanes.erase(std::unique(lanes.begin(), lanes.end()), lanes.end());
}

void sortUniqueResources(std::vector<FeatureLabel> &resources) {
  std::sort(resources.begin(), resources.end());
  resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
}

bool isTerminal(const ActionTaskState state) {
  return state == ActionTaskState::Finished || state == ActionTaskState::Cancelled;
}

std::uint32_t priorityValue(const ActionTaskPriority priority) {
  return static_cast<std::uint32_t>(priority);
}

std::uint64_t stableStringStamp(const std::string_view value,
                                std::uint64_t seed = 1469598103934665603ull) {
  for (const char c : value) {
    seed ^= static_cast<unsigned char>(c);
    seed *= 1099511628211ull;
  }
  return seed;
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

bool ActionResourceSet::add(FeatureLabel resource) {
  if (!resource.valid() || contains(resource)) {
    return false;
  }
  resources_.push_back(std::move(resource));
  sortUniqueResources(resources_);
  return true;
}

bool ActionResourceSet::add(const std::string_view resource) {
  const std::optional<FeatureLabel> parsed = parseFeatureLabel(resource);
  return parsed ? add(*parsed) : false;
}

bool ActionResourceSet::remove(const FeatureLabel &resource) {
  const auto found = std::find(resources_.begin(), resources_.end(), resource);
  if (found == resources_.end()) {
    return false;
  }
  resources_.erase(found);
  return true;
}

void ActionResourceSet::clear() {
  resources_.clear();
}

bool ActionResourceSet::empty() const noexcept {
  return resources_.empty();
}

std::size_t ActionResourceSet::size() const noexcept {
  return resources_.size();
}

bool ActionResourceSet::contains(const FeatureLabel &resource) const {
  return std::find(resources_.begin(), resources_.end(), resource) != resources_.end();
}

bool ActionResourceSet::overlaps(const ActionResourceSet &other) const {
  for (const FeatureLabel &resource : resources_) {
    if (other.contains(resource)) {
      return true;
    }
  }
  return false;
}

std::vector<FeatureLabel> ActionResourceSet::resources() const {
  return resources_;
}

std::uint64_t ActionResourceSet::stamp() const {
  std::uint64_t result = 0xA57EA110C1A10001ull;
  for (const FeatureLabel &resource : resources_) {
    result = hashCombine64(result, resource.stableId());
  }
  return result;
}

std::string ActionResourceSet::debugString() const {
  std::ostringstream out;
  for (std::size_t i = 0u; i < resources_.size(); ++i) {
    if (i > 0u) {
      out << ",";
    }
    out << resources_[i].path();
  }
  return out.str();
}

void ActionTaskJournal::record(ActionTaskJournalEntry entry) {
  entries_.push_back(std::move(entry));
}

void ActionTaskJournal::clear() {
  entries_.clear();
}

bool ActionTaskJournal::empty() const noexcept {
  return entries_.empty();
}

std::size_t ActionTaskJournal::size() const noexcept {
  return entries_.size();
}

const std::vector<ActionTaskJournalEntry> &ActionTaskJournal::entries() const noexcept {
  return entries_;
}

std::vector<ActionTaskJournalEntry> ActionTaskJournal::entriesFor(const ActionTaskId task_id) const {
  std::vector<ActionTaskJournalEntry> out;
  for (const ActionTaskJournalEntry &entry : entries_) {
    if (entry.task_id == task_id) {
      out.push_back(entry);
    }
  }
  return out;
}

std::uint64_t ActionTaskJournal::contractStamp() const {
  std::uint64_t result = 0xA57EA110FEE10001ull;
  for (const ActionTaskJournalEntry &entry : entries_) {
    result = hashCombine64(result, static_cast<std::uint64_t>(entry.kind));
    result = hashCombine64(result, entry.task_id);
    result = hashCombine64(result, entry.sequence);
    result = hashCombine64(result, entry.deterministic_stamp);
    result = hashCombine64(result, stableStringStamp(entry.detail));
  }
  return result;
}

std::string ActionTaskJournal::summary() const {
  std::map<std::string, std::size_t> by_owner;
  for (const ActionTaskJournalEntry &entry : entries_) {
    ++by_owner[entry.owner];
  }
  std::ostringstream out;
  out << "events=" << entries_.size();
  for (const auto &[owner, count] : by_owner) {
    out << " owner[" << (owner.empty() ? "none" : owner) << "]=" << count;
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
  node.record.required_resources = std::move(desc.required_resources);
  node.record.claimed_resources = std::move(desc.claimed_resources);
  node.record.labels = std::move(desc.labels);
  node.record.duration_seconds = desc.duration_seconds;
  node.record.sequence = next_sequence_++;
  node.record.can_preempt = desc.can_preempt;
  node.record.deterministic_stamp = deterministicStamp(node.record);
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

    std::vector<std::size_t> conflicts = conflictingActiveTasks(task.record);
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
        conflicts = conflictingActiveTasks(task.record);
      }
    }
    if (conflicts.empty()) {
      activateTask(index);
    } else {
      task.record.blocked_by_tasks.clear();
      for (const std::size_t conflict : conflicts) {
        task.record.blocked_by_tasks.push_back(tasks_[conflict].record.id);
      }
      task.record.diagnostic = "blocked by active action resource";
      task.record.owner_diagnostic = "owner '" + task.record.owner +
                                     "' is waiting on " +
                                     std::to_string(task.record.blocked_by_tasks.size()) +
                                     " active task(s)";
      if (task.record.state != ActionTaskState::Blocked) {
        task.record.state = ActionTaskState::Blocked;
        emit(ActionTaskEventKind::Blocked, task, task.record.diagnostic);
      }
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
  deferred_events_.clear();
  journal_.clear();
  world_labels_.clear();
  next_id_ = 1u;
  next_sequence_ = 1u;
  next_event_sequence_ = 1u;
  event_batch_depth_ = 0u;
  time_seconds_ = 0.0f;
}

void ActionScheduler::beginEventBatch() {
  ++event_batch_depth_;
}

void ActionScheduler::endEventBatch() {
  if (event_batch_depth_ == 0u) {
    return;
  }
  --event_batch_depth_;
  if (event_batch_depth_ == 0u) {
    flushDeferredEvents();
  }
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

const ActionTaskJournal &ActionScheduler::journal() const noexcept {
  return journal_;
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

ActionResourceSet ActionScheduler::claimedResources() const {
  ActionResourceSet resources;
  for (const ActionTaskNode &task : tasks_) {
    if (task.record.state != ActionTaskState::Active) {
      continue;
    }
    for (const FeatureLabel &resource : resourceFootprint(task.record).resources()) {
      resources.add(resource);
    }
  }
  return resources;
}

std::vector<ActionOwnerDiagnostic> ActionScheduler::ownerDiagnostics() const {
  std::map<std::string, ActionOwnerDiagnostic> by_owner;
  for (const ActionTaskNode &task : tasks_) {
    ActionOwnerDiagnostic &diagnostic = by_owner[task.record.owner];
    diagnostic.owner = task.record.owner;
    switch (task.record.state) {
    case ActionTaskState::Waiting:
      ++diagnostic.waiting_tasks;
      break;
    case ActionTaskState::Blocked:
      ++diagnostic.blocked_tasks;
      break;
    case ActionTaskState::Active:
      ++diagnostic.active_tasks;
      break;
    case ActionTaskState::Finished:
      ++diagnostic.finished_tasks;
      break;
    case ActionTaskState::Cancelled:
      ++diagnostic.cancelled_tasks;
      break;
    }
    diagnostic.lane_stamp = hashCombine64(diagnostic.lane_stamp, task.record.lanes.stamp());
    diagnostic.resource_stamp =
        hashCombine64(diagnostic.resource_stamp, resourceFootprint(task.record).stamp());
    if (!task.record.diagnostic.empty()) {
      diagnostic.diagnostics.push_back(task.record.name + ": " + task.record.diagnostic);
    }
  }
  std::vector<ActionOwnerDiagnostic> out;
  out.reserve(by_owner.size());
  for (auto &[owner, diagnostic] : by_owner) {
    out.push_back(std::move(diagnostic));
  }
  return out;
}

std::optional<std::size_t> ActionScheduler::indexOf(const ActionTaskId id) const {
  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    if (tasks_[index].record.id == id) {
      return index;
    }
  }
  return std::nullopt;
}

std::vector<std::size_t> ActionScheduler::conflictingActiveTasks(
    const ActionTaskRecord &record) const {
  std::vector<std::size_t> conflicts;
  const ActionResourceSet resources = resourceFootprint(record);
  if (record.lanes.empty() && resources.empty()) {
    return conflicts;
  }
  for (std::size_t index = 0u; index < tasks_.size(); ++index) {
    if (tasks_[index].record.id == record.id ||
        tasks_[index].record.state != ActionTaskState::Active) {
      continue;
    }
    const bool lane_conflict =
        !record.lanes.empty() && tasks_[index].record.lanes.overlaps(record.lanes);
    const bool resource_conflict =
        !resources.empty() && resourceFootprint(tasks_[index].record).overlaps(resources);
    if (lane_conflict || resource_conflict) {
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
                           node.record.labels,
                           node.record.required_resources,
                           node.record.claimed_resources};
}

ActionResourceSet ActionScheduler::resourceFootprint(const ActionTaskRecord &record) const {
  ActionResourceSet resources;
  for (const FeatureLabel &resource : record.required_resources.resources()) {
    resources.add(resource);
  }
  for (const FeatureLabel &resource : record.claimed_resources.resources()) {
    resources.add(resource);
  }
  return resources;
}

std::uint64_t ActionScheduler::deterministicStamp(const ActionTaskRecord &record) const {
  std::uint64_t stamp = 0xA57EA1105CED0001ull;
  stamp = hashCombine64(stamp, record.id);
  stamp = hashCombine64(stamp, record.sequence);
  stamp = hashCombine64(stamp, static_cast<std::uint64_t>(record.priority));
  stamp = hashCombine64(stamp, record.lanes.stamp());
  stamp = hashCombine64(stamp, resourceFootprint(record).stamp());
  stamp = hashCombine64(stamp, record.labels.contractStamp());
  stamp = hashCombine64(stamp, stableStringStamp(record.name));
  stamp = hashCombine64(stamp, stableStringStamp(record.owner));
  return stamp;
}

void ActionScheduler::activateTask(const std::size_t index) {
  ActionTaskNode &task = tasks_[index];
  if (task.record.state == ActionTaskState::Active || isTerminal(task.record.state)) {
    return;
  }
  task.record.state = ActionTaskState::Active;
  task.record.diagnostic.clear();
  task.record.owner_diagnostic.clear();
  task.record.blocked_by_tasks.clear();
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
  const ActionResourceSet resources = resourceFootprint(node.record);
  ActionTaskEvent event{.kind = kind,
                        .task_id = node.record.id,
                        .task_name = node.record.name,
                        .owner = node.record.owner,
                        .state = node.record.state,
                        .time_seconds = time_seconds_,
                        .sequence = next_event_sequence_++,
                        .lane_stamp = node.record.lanes.stamp(),
                        .resource_stamp = resources.stamp(),
                        .detail = std::move(detail)};
  journal_.record({.kind = event.kind,
                   .task_id = event.task_id,
                   .task_name = event.task_name,
                   .owner = event.owner,
                   .state = event.state,
                   .time_seconds = event.time_seconds,
                   .sequence = event.sequence,
                   .deterministic_stamp = deterministicStamp(node.record),
                   .lane_stamp = event.lane_stamp,
                   .resource_stamp = event.resource_stamp,
                   .detail = event.detail});
  if (event_batch_depth_ > 0u) {
    deferred_events_.push_back(std::move(event));
  } else {
    events_.push_back(std::move(event));
  }
}

void ActionScheduler::flushDeferredEvents() {
  events_.insert(events_.end(), deferred_events_.begin(), deferred_events_.end());
  deferred_events_.clear();
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
