// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace aster {

struct FrameWorkClassBudget {
  std::uint32_t class_id = 0u;
  std::uint32_t max_items = 0u;
  double max_seconds = 0.0;
};

struct FrameWorkBudget {
  std::uint32_t max_items = 0u;
  double max_seconds = 0.0;
  std::uint32_t starvation_frame_limit = 0u;
  double starvation_priority_per_frame = 0.0;
  std::vector<FrameWorkClassBudget> class_budgets;
};

struct BudgetedWorkItem {
  std::uint64_t id = 0u;
  std::uint32_t class_id = 0u;
  double priority = 0.0;
  double estimated_seconds = 0.0;
  std::uint32_t virtual_backlog_frames = 0u;
  std::uint64_t sequence = 0u;
  std::size_t payload_index = 0u;
};

struct BudgetedWorkDiagnostics {
  std::size_t queued_items = 0u;
  std::size_t selected_items = 0u;
  std::size_t deferred_items = 0u;
  std::size_t budget_exhausted_items = 0u;
  double queued_seconds = 0.0;
  double selected_seconds = 0.0;
  std::uint32_t max_virtual_backlog_frames = 0u;
};

struct WorkCostSample {
  std::uint32_t class_id = 0u;
  double seconds = 0.0;
};

struct WorkCostEstimate {
  std::uint32_t class_id = 0u;
  double mean_seconds = 0.0;
  double peak_seconds = 0.0;
  std::uint64_t sample_count = 0u;
};

class WorkCostModel {
public:
  void clear();
  void observe(WorkCostSample sample);
  [[nodiscard]] double estimate(std::uint32_t class_id, double fallback_seconds) const;
  [[nodiscard]] std::optional<WorkCostEstimate> estimateFor(std::uint32_t class_id) const;
  [[nodiscard]] std::vector<WorkCostEstimate> estimates() const;

private:
  struct ClassEstimate {
    double mean_seconds = 0.0;
    double peak_seconds = 0.0;
    std::uint64_t sample_count = 0u;
  };

  std::unordered_map<std::uint32_t, ClassEstimate> estimates_;
};

struct BudgetTelemetry {
  double target_frame_seconds = 1.0 / 60.0;
  double frame_seconds = 0.0;
  double update_seconds = 0.0;
  double render_seconds = 0.0;
  double upload_seconds = 0.0;
  double available_work_seconds = 0.0;
  double pressure = 0.0;
  double backlog_pressure = 0.0;
  std::uint32_t backlog_items = 0u;
  std::uint32_t requested_items = 0u;
};

struct FrameBudgetControllerOptions {
  double target_frame_seconds = 1.0 / 60.0;
  double min_work_seconds = 0.00025;
  double max_work_seconds = 0.0045;
  std::uint32_t min_items = 1u;
  std::uint32_t max_items = 4u;
  double backlog_full_scale = 32.0;
  double recovery_rate = 0.20;
  double pressure_rate = 0.35;
  double starvation_priority_per_frame = 0.012;
  std::uint32_t starvation_frame_limit = 18u;
};

struct FrameBudgetControllerInput {
  double frame_seconds = 0.0;
  double update_seconds = 0.0;
  double render_seconds = 0.0;
  double upload_seconds = 0.0;
  std::uint32_t backlog_items = 0u;
  double viewer_speed = 0.0;
};

class FrameBudgetController {
public:
  explicit FrameBudgetController(FrameBudgetControllerOptions options = {});

  void reset();
  [[nodiscard]] FrameWorkBudget nextBudget(const FrameBudgetControllerInput &input,
                                           const FrameWorkBudget &base_budget);
  [[nodiscard]] const BudgetTelemetry &telemetry() const {
    return telemetry_;
  }

private:
  FrameBudgetControllerOptions options_{};
  BudgetTelemetry telemetry_{};
  double pressure_state_ = 0.0;
};

struct BudgetedWorkSelection {
  std::vector<BudgetedWorkItem> selected;
  std::vector<BudgetedWorkItem> deferred;
  BudgetedWorkDiagnostics diagnostics{};
};

class BudgetedWorkQueue {
public:
  void clear();
  void reserve(std::size_t count);
  void enqueue(BudgetedWorkItem item);
  [[nodiscard]] BudgetedWorkSelection select(const FrameWorkBudget &budget) const;
  [[nodiscard]] const std::vector<BudgetedWorkItem> &items() const {
    return items_;
  }

private:
  std::vector<BudgetedWorkItem> items_;
};

} // namespace aster
