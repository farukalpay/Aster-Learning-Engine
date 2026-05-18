// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct TimestampQueryDesc {
  std::uint32_t slot_count = 0u;
  std::string debug_label;
};

} // namespace aster::rhi
