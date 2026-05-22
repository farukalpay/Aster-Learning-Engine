// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aster::graphics_core7 {

struct VisibilityCompactionInput {
  std::size_t object_index = 0u;
  bool visible = false;
  std::uint64_t draw_key = 0u;
};

struct VisibilityCompactionReport {
  std::size_t input_count = 0u;
  std::size_t visible_count = 0u;
  std::uint64_t draw_key_hash = 0u;
  std::vector<std::size_t> visible_indices;
};

[[nodiscard]] VisibilityCompactionReport
compactGpuVisibility(const std::vector<VisibilityCompactionInput> &inputs);

} // namespace aster::graphics_core7
