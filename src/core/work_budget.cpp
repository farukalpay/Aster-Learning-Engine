// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/work_budget.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace aster {
namespace {

struct ClassUse {
  std::uint32_t items = 0u;
  double seconds = 0.0;
};

[[nodiscard]] double normalizedCost(const double seconds) {
  return std::max(seconds, 0.0);
}

[[nodiscard]] double effectivePriority(const BudgetedWorkItem &item,
                                       const FrameWorkBudget &budget) {
  return item.priority + static_cast<double>(item.virtual_backlog_frames) *
                             std::max(budget.starvation_priority_per_frame, 0.0);
}

[[nodiscard]] bool isStarved(const BudgetedWorkItem &item, const FrameWorkBudget &budget) {
  return budget.starvation_frame_limit > 0u &&
         item.virtual_backlog_frames >= budget.starvation_frame_limit;
}

[[nodiscard]] const FrameWorkClassBudget *
classBudgetFor(const FrameWorkBudget &budget, const std::uint32_t class_id) {
  const auto found =
      std::find_if(budget.class_budgets.begin(), budget.class_budgets.end(),
                   [class_id](const FrameWorkClassBudget &entry) {
                     return entry.class_id == class_id;
                   });
  return found == budget.class_budgets.end() ? nullptr : &*found;
}

[[nodiscard]] bool fitsFrameBudget(const FrameWorkBudget &budget,
                                   const BudgetedWorkDiagnostics &diagnostics,
                                   const BudgetedWorkItem &item) {
  if (budget.max_items > 0u && diagnostics.selected_items >= budget.max_items) {
    return false;
  }
  const double cost = normalizedCost(item.estimated_seconds);
  return budget.max_seconds <= 0.0 || diagnostics.selected_seconds + cost <= budget.max_seconds;
}

[[nodiscard]] bool fitsClassBudget(const FrameWorkClassBudget *budget, const ClassUse &used,
                                   const BudgetedWorkItem &item) {
  if (budget == nullptr) {
    return true;
  }
  if (budget->max_items > 0u && used.items >= budget->max_items) {
    return false;
  }
  const double cost = normalizedCost(item.estimated_seconds);
  return budget->max_seconds <= 0.0 || used.seconds + cost <= budget->max_seconds;
}

} // namespace

void BudgetedWorkQueue::clear() {
  items_.clear();
}

void BudgetedWorkQueue::reserve(const std::size_t count) {
  items_.reserve(count);
}

void BudgetedWorkQueue::enqueue(BudgetedWorkItem item) {
  item.estimated_seconds = normalizedCost(item.estimated_seconds);
  items_.push_back(item);
}

void WorkCostModel::clear() {
  estimates_.clear();
}

void WorkCostModel::observe(WorkCostSample sample) {
  const double seconds = normalizedCost(sample.seconds);
  if (seconds <= 0.0) {
    return;
  }

  ClassEstimate &estimate = estimates_[sample.class_id];
  ++estimate.sample_count;
  if (estimate.sample_count == 1u) {
    estimate.mean_seconds = seconds;
    estimate.peak_seconds = seconds;
    return;
  }

  constexpr double kMeanBlend = 0.18;
  constexpr double kPeakDecay = 0.96;
  estimate.mean_seconds += (seconds - estimate.mean_seconds) * kMeanBlend;
  estimate.peak_seconds = std::max(seconds, estimate.peak_seconds * kPeakDecay);
}

double WorkCostModel::estimate(const std::uint32_t class_id,
                               const double fallback_seconds) const {
  const auto found = estimates_.find(class_id);
  if (found == estimates_.end() || found->second.sample_count == 0u) {
    return normalizedCost(fallback_seconds);
  }
  return std::max(found->second.mean_seconds, normalizedCost(fallback_seconds));
}

std::optional<WorkCostEstimate> WorkCostModel::estimateFor(const std::uint32_t class_id) const {
  const auto found = estimates_.find(class_id);
  if (found == estimates_.end()) {
    return std::nullopt;
  }
  return WorkCostEstimate{.class_id = class_id,
                          .mean_seconds = found->second.mean_seconds,
                          .peak_seconds = found->second.peak_seconds,
                          .sample_count = found->second.sample_count};
}

std::vector<WorkCostEstimate> WorkCostModel::estimates() const {
  std::vector<WorkCostEstimate> out;
  out.reserve(estimates_.size());
  for (const auto &entry : estimates_) {
    out.push_back({.class_id = entry.first,
                   .mean_seconds = entry.second.mean_seconds,
                   .peak_seconds = entry.second.peak_seconds,
                   .sample_count = entry.second.sample_count});
  }
  std::sort(out.begin(), out.end(), [](const WorkCostEstimate &lhs,
                                       const WorkCostEstimate &rhs) {
    return lhs.class_id < rhs.class_id;
  });
  return out;
}

FrameBudgetController::FrameBudgetController(FrameBudgetControllerOptions options)
    : options_(options) {
  options_.target_frame_seconds = std::max(options_.target_frame_seconds, 0.001);
  options_.min_work_seconds = normalizedCost(options_.min_work_seconds);
  options_.max_work_seconds = std::max(options_.max_work_seconds, options_.min_work_seconds);
  options_.max_items = std::max(options_.max_items, options_.min_items);
  options_.backlog_full_scale = std::max(options_.backlog_full_scale, 1.0);
  options_.recovery_rate = std::clamp(options_.recovery_rate, 0.0, 1.0);
  options_.pressure_rate = std::clamp(options_.pressure_rate, 0.0, 1.0);
}

void FrameBudgetController::reset() {
  telemetry_ = {};
  telemetry_.target_frame_seconds = options_.target_frame_seconds;
  pressure_state_ = 0.0;
}

FrameWorkBudget FrameBudgetController::nextBudget(const FrameBudgetControllerInput &input,
                                                  const FrameWorkBudget &base_budget) {
  telemetry_.target_frame_seconds = options_.target_frame_seconds;
  telemetry_.frame_seconds = normalizedCost(input.frame_seconds);
  telemetry_.update_seconds = normalizedCost(input.update_seconds);
  telemetry_.render_seconds = normalizedCost(input.render_seconds);
  telemetry_.upload_seconds = normalizedCost(input.upload_seconds);
  telemetry_.backlog_items = input.backlog_items;

  const double observed_frame_seconds =
      telemetry_.frame_seconds > 0.0
          ? telemetry_.frame_seconds
          : telemetry_.update_seconds + telemetry_.render_seconds + telemetry_.upload_seconds;
  const double overload =
      std::max(0.0, observed_frame_seconds - options_.target_frame_seconds) /
      options_.target_frame_seconds;
  const double spare =
      std::max(0.0, options_.target_frame_seconds - observed_frame_seconds) /
      options_.target_frame_seconds;
  pressure_state_ = std::clamp(pressure_state_ + overload * options_.pressure_rate -
                                   spare * options_.recovery_rate,
                               0.0, 1.0);
  telemetry_.pressure = pressure_state_;
  telemetry_.backlog_pressure =
      std::clamp(static_cast<double>(input.backlog_items) / options_.backlog_full_scale, 0.0, 1.0);

  const double speed_gain =
      std::clamp(normalizedCost(input.viewer_speed) / std::max(options_.backlog_full_scale, 1.0),
                 0.0, 0.35);
  const double availability =
      std::clamp(1.0 - pressure_state_ + telemetry_.backlog_pressure * 0.35 + speed_gain, 0.0, 1.0);
  telemetry_.available_work_seconds =
      options_.min_work_seconds +
      (options_.max_work_seconds - options_.min_work_seconds) * availability;
  telemetry_.requested_items = static_cast<std::uint32_t>(std::round(
      static_cast<double>(options_.min_items) +
      static_cast<double>(options_.max_items - options_.min_items) * availability));
  telemetry_.requested_items =
      std::clamp(telemetry_.requested_items, options_.min_items, options_.max_items);

  FrameWorkBudget budget = base_budget;
  budget.max_items = budget.max_items == 0u ? telemetry_.requested_items
                                            : std::min(budget.max_items, telemetry_.requested_items);
  budget.max_seconds = budget.max_seconds <= 0.0
                           ? telemetry_.available_work_seconds
                           : std::min(budget.max_seconds, telemetry_.available_work_seconds);
  budget.starvation_frame_limit =
      budget.starvation_frame_limit == 0u ? options_.starvation_frame_limit
                                          : budget.starvation_frame_limit;
  budget.starvation_priority_per_frame =
      budget.starvation_priority_per_frame <= 0.0 ? options_.starvation_priority_per_frame
                                                  : budget.starvation_priority_per_frame;
  for (FrameWorkClassBudget &class_budget : budget.class_budgets) {
    if (class_budget.max_items == 0u || class_budget.max_items > budget.max_items) {
      class_budget.max_items = budget.max_items;
    }
    if (class_budget.max_seconds <= 0.0 || class_budget.max_seconds > budget.max_seconds) {
      class_budget.max_seconds = budget.max_seconds;
    }
  }
  return budget;
}

BudgetedWorkSelection BudgetedWorkQueue::select(const FrameWorkBudget &budget) const {
  BudgetedWorkSelection selection;
  selection.diagnostics.queued_items = items_.size();
  selection.selected.reserve(items_.size());
  selection.deferred.reserve(items_.size());

  std::vector<BudgetedWorkItem> ordered = items_;
  for (const BudgetedWorkItem &item : ordered) {
    selection.diagnostics.queued_seconds += normalizedCost(item.estimated_seconds);
    selection.diagnostics.max_virtual_backlog_frames =
        std::max(selection.diagnostics.max_virtual_backlog_frames,
                 item.virtual_backlog_frames);
  }

  std::sort(ordered.begin(), ordered.end(),
            [&budget](const BudgetedWorkItem &lhs, const BudgetedWorkItem &rhs) {
              const bool lhs_starved = isStarved(lhs, budget);
              const bool rhs_starved = isStarved(rhs, budget);
              if (lhs_starved != rhs_starved) {
                return lhs_starved;
              }
              const double lhs_priority = effectivePriority(lhs, budget);
              const double rhs_priority = effectivePriority(rhs, budget);
              if (std::abs(lhs_priority - rhs_priority) > 0.0000001) {
                return lhs_priority > rhs_priority;
              }
              if (lhs.virtual_backlog_frames != rhs.virtual_backlog_frames) {
                return lhs.virtual_backlog_frames > rhs.virtual_backlog_frames;
              }
              if (lhs.class_id != rhs.class_id) {
                return lhs.class_id < rhs.class_id;
              }
              if (lhs.id != rhs.id) {
                return lhs.id < rhs.id;
              }
              return lhs.sequence < rhs.sequence;
            });

  std::unordered_map<std::uint32_t, ClassUse> class_use;
  class_use.reserve(budget.class_budgets.size());
  for (const BudgetedWorkItem &item : ordered) {
    ClassUse &used = class_use[item.class_id];
    const FrameWorkClassBudget *class_budget = classBudgetFor(budget, item.class_id);
    if (!fitsFrameBudget(budget, selection.diagnostics, item) ||
        !fitsClassBudget(class_budget, used, item)) {
      selection.deferred.push_back(item);
      ++selection.diagnostics.budget_exhausted_items;
      continue;
    }

    selection.selected.push_back(item);
    ++selection.diagnostics.selected_items;
    selection.diagnostics.selected_seconds += normalizedCost(item.estimated_seconds);
    ++used.items;
    used.seconds += normalizedCost(item.estimated_seconds);
  }

  selection.diagnostics.deferred_items = selection.deferred.size();
  return selection;
}

} // namespace aster
