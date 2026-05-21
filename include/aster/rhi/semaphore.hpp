// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/rhi/handle.hpp"

#include <string>

namespace aster::rhi {

struct SemaphoreDesc {
  std::string debug_label;
};

} // namespace aster::rhi
