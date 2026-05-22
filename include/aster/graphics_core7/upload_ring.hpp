// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster::graphics_core7 {

struct UploadRingAllocation {
  std::size_t offset = 0u;
  std::size_t byte_size = 0u;
  std::size_t aligned_byte_size = 0u;
  std::uint64_t fence_value = 0u;
  std::string label;
};

struct UploadRingStats {
  std::size_t capacity_bytes = 0u;
  std::size_t used_bytes = 0u;
  std::size_t allocation_count = 0u;
  std::size_t wrap_count = 0u;
  bool overflow = false;
};

class UploadRing {
public:
  explicit UploadRing(std::size_t capacity_bytes = 0u, std::size_t alignment = 256u);
  [[nodiscard]] UploadRingAllocation allocate(std::size_t byte_size, std::string label = {});
  void beginFrame(std::uint64_t completed_fence_value);
  [[nodiscard]] const UploadRingStats &stats() const noexcept;
  [[nodiscard]] const std::vector<UploadRingAllocation> &allocations() const noexcept;

private:
  std::size_t capacity_bytes_ = 0u;
  std::size_t alignment_ = 256u;
  std::size_t cursor_ = 0u;
  UploadRingStats stats_{};
  std::vector<UploadRingAllocation> allocations_;
};

} // namespace aster::graphics_core7
