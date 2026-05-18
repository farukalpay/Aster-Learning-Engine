// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/framegraph/graph_compiler.hpp"

namespace aster::framegraph {

[[nodiscard]] inline std::string graphVisualizerDump(const CompiledFrameGraph &graph) {
  return dumpFrameGraph(graph);
}

} // namespace aster::framegraph
