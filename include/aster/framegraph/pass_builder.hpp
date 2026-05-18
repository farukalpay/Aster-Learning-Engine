// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/resource_handle.hpp"

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
  std::vector<ResourceHandle> reads;
  std::vector<ResourceHandle> writes;
};

class FrameGraph;

class PassBuilder {
public:
  PassBuilder(FrameGraph &graph, std::size_t pass_index);

  PassBuilder &reads(ResourceHandle resource);
  PassBuilder &writes(ResourceHandle resource);

private:
  FrameGraph *graph_ = nullptr;
  std::size_t pass_index_ = 0u;
};

} // namespace aster::framegraph
