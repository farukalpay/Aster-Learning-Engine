// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include <cstddef>

namespace aster {

struct FixedTimestepConfig {
  double step_seconds = 1.0 / 60.0;
  double max_frame_seconds = 1.0 / 20.0;
  std::size_t max_steps_per_frame = 4;
};

class FixedTimestep {
public:
  explicit FixedTimestep(FixedTimestepConfig config = {});

  [[nodiscard]] std::size_t advance(double frame_seconds);
  [[nodiscard]] double stepSeconds() const;
  [[nodiscard]] double accumulatorSeconds() const;
  [[nodiscard]] double interpolationAlpha() const;

  void reset();

private:
  FixedTimestepConfig config_{};
  double accumulator_seconds_ = 0.0;
};

} // namespace aster
