// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/rhi/handle.hpp"

#include <cstdint>
#include <string>

namespace aster::rhi {

struct FenceDesc {
  std::uint64_t initial_value = 0u;
  std::string debug_label;
};

} // namespace aster::rhi
