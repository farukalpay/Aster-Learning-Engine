// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster::graphics_core7 {

struct DescriptorArenaRange {
  std::uint32_t first = 0u;
  std::uint32_t count = 0u;
  std::string label;
};

struct DescriptorArenaStats {
  std::uint32_t capacity = 0u;
  std::uint32_t used = 0u;
  std::uint32_t high_watermark = 0u;
  std::uint32_t allocation_count = 0u;
  bool overflow = false;
};

class DescriptorArena {
public:
  explicit DescriptorArena(std::uint32_t capacity = 0u);
  [[nodiscard]] DescriptorArenaRange allocate(std::uint32_t count, std::string label = {});
  void reset();
  [[nodiscard]] const DescriptorArenaStats &stats() const noexcept;
  [[nodiscard]] const std::vector<DescriptorArenaRange> &ranges() const noexcept;

private:
  DescriptorArenaStats stats_{};
  std::vector<DescriptorArenaRange> ranges_;
};

} // namespace aster::graphics_core7
