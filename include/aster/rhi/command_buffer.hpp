// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/queue.hpp"

#include <string>

namespace aster::rhi {

struct CommandBufferDesc {
  QueueKind queue = QueueKind::Graphics;
  bool one_time_submit = false;
  std::string debug_label;
};

} // namespace aster::rhi
