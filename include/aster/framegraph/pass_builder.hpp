// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/resource_handle.hpp"
#include "aster/rhi/queue.hpp"

#include <string>
#include <vector>

namespace aster::framegraph {

enum class ResourceAccess {
  Read,
  Write,
};

struct ResourceUsage {
  ResourceHandle resource{};
  ResourceAccess access = ResourceAccess::Read;
};

struct PassDesc {
  std::string name;
  std::string event_label;
  std::string event_scope;
  std::vector<ResourceHandle> reads;
  std::vector<ResourceHandle> writes;
  rhi::QueueKind queue = rhi::QueueKind::Graphics;
  bool allow_read_write_overlap = false;
};

class FrameGraph;

class PassBuilder {
public:
  PassBuilder(FrameGraph &graph, std::size_t pass_index);

  PassBuilder &reads(ResourceHandle resource);
  PassBuilder &writes(ResourceHandle resource);
  PassBuilder &queue(rhi::QueueKind queue);
  PassBuilder &eventLabel(std::string label);
  PassBuilder &eventScope(std::string scope);
  PassBuilder &allowReadWriteOverlap(bool allowed = true);

private:
  FrameGraph *graph_ = nullptr;
  std::size_t pass_index_ = 0u;
};

} // namespace aster::framegraph
