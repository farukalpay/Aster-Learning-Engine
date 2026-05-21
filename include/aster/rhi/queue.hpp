// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstdint>
#include <string>

namespace aster::rhi {

enum class QueueKind : std::uint32_t {
  Graphics,
  Compute,
  Copy,
};

struct QueueDesc {
  QueueKind kind = QueueKind::Graphics;
  std::string debug_label;
};

} // namespace aster::rhi
