// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

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
