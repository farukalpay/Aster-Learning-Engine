// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

namespace aster {

class Clock {
public:
  Clock();

  double tick();
  [[nodiscard]] double now() const;

private:
  double previous_seconds_ = 0.0;
};

} // namespace aster
