// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace aster {

struct FrameTimeSummary {
  std::size_t samples = 0;
  double min_seconds = 0.0;
  double mean_seconds = 0.0;
  double p95_seconds = 0.0;
  double max_seconds = 0.0;
  std::optional<double> budget_seconds;
  std::size_t over_budget = 0;
};

class FrameTimeStats {
public:
  void addSample(double seconds);
  void clear();

  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::size_t sampleCount() const;
  [[nodiscard]] FrameTimeSummary summarize(std::optional<double> budget_seconds = {}) const;

private:
  std::vector<double> samples_;
};

} // namespace aster
