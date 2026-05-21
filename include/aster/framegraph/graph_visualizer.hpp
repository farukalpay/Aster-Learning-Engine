// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/framegraph/graph_compiler.hpp"

namespace aster::framegraph {

[[nodiscard]] inline std::string graphVisualizerDump(const CompiledFrameGraph &graph) {
  return dumpFrameGraph(graph);
}

} // namespace aster::framegraph
