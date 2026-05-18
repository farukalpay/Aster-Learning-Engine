// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/frame_graph.hpp"
#include "aster/rhi/resource_barrier.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace aster::framegraph {

struct CompiledResource {
  ResourceHandle handle{};
  std::string name;
  ResourceDesc desc{};
  std::size_t first_pass = 0u;
  std::size_t last_pass = 0u;
};

struct CompiledPass {
  std::string name;
  std::vector<ResourceHandle> reads;
  std::vector<ResourceHandle> writes;
  std::uint32_t read_mask = 0u;
  std::uint32_t write_mask = 0u;
};

struct GraphBarrier {
  ResourceHandle resource{};
  rhi::ResourceState before = rhi::ResourceState::Undefined;
  rhi::ResourceState after = rhi::ResourceState::Undefined;
  std::size_t pass_index = 0u;
};

struct CompiledFrameGraph {
  std::vector<CompiledResource> resources;
  std::vector<CompiledPass> passes;
  std::vector<GraphBarrier> barriers;
  std::vector<std::string> validation_errors;
  std::size_t transient_resource_count = 0u;

  [[nodiscard]] bool valid() const {
    return validation_errors.empty();
  }
};

[[nodiscard]] CompiledFrameGraph compileFrameGraph(const FrameGraph &graph);
[[nodiscard]] std::string dumpFrameGraph(const CompiledFrameGraph &graph);

} // namespace aster::framegraph
