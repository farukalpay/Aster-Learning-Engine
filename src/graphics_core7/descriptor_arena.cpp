// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/descriptor_arena.hpp"

#include <algorithm>
#include <utility>

namespace aster::graphics_core7 {

DescriptorArena::DescriptorArena(const std::uint32_t capacity) {
  stats_.capacity = capacity;
}

DescriptorArenaRange DescriptorArena::allocate(const std::uint32_t count, std::string label) {
  if (count == 0u) {
    return {.first = stats_.used, .label = std::move(label)};
  }
  if (stats_.used + count > stats_.capacity) {
    stats_.overflow = true;
    return {.first = stats_.used, .count = count, .label = std::move(label)};
  }
  DescriptorArenaRange range{.first = stats_.used, .count = count, .label = std::move(label)};
  stats_.used += count;
  stats_.high_watermark = std::max(stats_.high_watermark, stats_.used);
  ++stats_.allocation_count;
  ranges_.push_back(range);
  return range;
}

void DescriptorArena::reset() {
  stats_.used = 0u;
  stats_.allocation_count = 0u;
  stats_.overflow = false;
  ranges_.clear();
}

const DescriptorArenaStats &DescriptorArena::stats() const noexcept {
  return stats_;
}

const std::vector<DescriptorArenaRange> &DescriptorArena::ranges() const noexcept {
  return ranges_;
}

} // namespace aster::graphics_core7
