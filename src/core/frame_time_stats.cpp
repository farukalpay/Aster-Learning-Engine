// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/core/frame_time_stats.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace aster {
namespace {

double nearestRankQuantile(std::vector<double> values, const double quantile) {
  if (values.empty()) {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  const double clamped = std::clamp(quantile, 0.0, 1.0);
  const std::size_t rank = std::max<std::size_t>(
      1u, static_cast<std::size_t>(std::ceil(clamped * static_cast<double>(values.size()))));
  const std::size_t index = rank - 1u;
  return values[std::min(index, values.size() - 1u)];
}

} // namespace

void FrameTimeStats::addSample(const double seconds) {
  if (std::isfinite(seconds) && seconds >= 0.0) {
    samples_.push_back(seconds);
  }
}

void FrameTimeStats::clear() {
  samples_.clear();
}

bool FrameTimeStats::empty() const {
  return samples_.empty();
}

std::size_t FrameTimeStats::sampleCount() const {
  return samples_.size();
}

FrameTimeSummary FrameTimeStats::summarize(const std::optional<double> budget_seconds) const {
  FrameTimeSummary summary;
  summary.samples = samples_.size();
  summary.budget_seconds = budget_seconds;

  if (samples_.empty()) {
    return summary;
  }

  const auto [min_it, max_it] = std::minmax_element(samples_.begin(), samples_.end());
  summary.min_seconds = *min_it;
  summary.max_seconds = *max_it;
  summary.mean_seconds =
      std::accumulate(samples_.begin(), samples_.end(), 0.0) / static_cast<double>(samples_.size());
  summary.p95_seconds = nearestRankQuantile(samples_, 0.95);

  if (budget_seconds.has_value()) {
    summary.over_budget = static_cast<std::size_t>(
        std::count_if(samples_.begin(), samples_.end(),
                      [budget_seconds](const double sample) { return sample > *budget_seconds; }));
  }

  return summary;
}

} // namespace aster
