// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/rhi/pipeline_layout.hpp"
#include "aster/rhi/resource_barrier.hpp"
#include "aster/rhi/timestamp_query.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aster::rhi {

struct DescriptorLayoutTrace {
  std::string label;
  std::uint64_t layout_hash = 0u;
  std::vector<DescriptorRangeDesc> ranges;
};

struct PipelineStateTrace {
  std::string label;
  std::uint64_t cache_key = 0u;
  std::uint64_t descriptor_layout_hash = 0u;
};

struct ResourceTransitionTrace {
  std::string pass;
  std::string resource;
  ResourceBarrier barrier{};
};

struct QueueSubmitTrace {
  std::string label;
  QueueKind queue = QueueKind::Graphics;
  std::size_t command_buffer_count = 0u;
  std::uint64_t signal_fence_value = 0u;
};

struct FenceTimelineTrace {
  std::string label;
  std::uint64_t submitted_value = 0u;
  std::uint64_t completed_value = 0u;
};

struct MemoryBudgetTrace {
  std::uint64_t budget_bytes = 0u;
  std::uint64_t resident_bytes = 0u;
  std::uint64_t transient_bytes = 0u;
};

struct FrameTrace {
  std::vector<DescriptorLayoutTrace> descriptor_layouts;
  std::vector<PipelineStateTrace> pipelines;
  std::vector<ResourceTransitionTrace> transitions;
  std::vector<QueueSubmitTrace> queue_submits;
  std::vector<FenceTimelineTrace> fences;
  std::vector<TimestampQueryResult> timestamps;
  MemoryBudgetTrace memory{};
};

} // namespace aster::rhi
