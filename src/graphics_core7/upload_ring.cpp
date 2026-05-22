// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#include "aster/graphics_core7/upload_ring.hpp"

#include <algorithm>
#include <utility>

namespace aster::graphics_core7 {
namespace {

[[nodiscard]] std::size_t alignTo(const std::size_t value, const std::size_t alignment) {
  return alignment == 0u ? value : ((value + alignment - 1u) / alignment) * alignment;
}

} // namespace

UploadRing::UploadRing(const std::size_t capacity_bytes, const std::size_t alignment)
    : capacity_bytes_(capacity_bytes), alignment_(std::max<std::size_t>(alignment, 1u)) {
  stats_.capacity_bytes = capacity_bytes_;
}

UploadRingAllocation UploadRing::allocate(const std::size_t byte_size, std::string label) {
  const std::size_t aligned = alignTo(byte_size, alignment_);
  if (capacity_bytes_ == 0u || aligned > capacity_bytes_) {
    stats_.overflow = true;
    return {.byte_size = byte_size, .aligned_byte_size = aligned, .label = std::move(label)};
  }
  if (cursor_ + aligned > capacity_bytes_) {
    cursor_ = 0u;
    ++stats_.wrap_count;
  }
  UploadRingAllocation allocation{.offset = cursor_,
                                  .byte_size = byte_size,
                                  .aligned_byte_size = aligned,
                                  .label = std::move(label)};
  cursor_ += aligned;
  stats_.used_bytes = std::max(stats_.used_bytes, cursor_);
  ++stats_.allocation_count;
  allocations_.push_back(allocation);
  return allocation;
}

void UploadRing::beginFrame(const std::uint64_t completed_fence_value) {
  (void)completed_fence_value;
  cursor_ = 0u;
  stats_.used_bytes = 0u;
  stats_.allocation_count = 0u;
  stats_.overflow = false;
  allocations_.clear();
}

const UploadRingStats &UploadRing::stats() const noexcept {
  return stats_;
}

const std::vector<UploadRingAllocation> &UploadRing::allocations() const noexcept {
  return allocations_;
}

} // namespace aster::graphics_core7
