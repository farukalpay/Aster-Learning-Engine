// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct TimestampQueryDesc {
  std::uint32_t slot_count = 0u;
  double timestamp_period_nanoseconds = 1.0;
  std::string debug_label;
};

struct TimestampQueryResult {
  std::uint32_t slot = 0u;
  std::uint64_t ticks = 0u;
  double nanoseconds = 0.0;
  bool available = false;
};

} // namespace aster::rhi
