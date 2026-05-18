// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/render/render_graph.hpp"

#include <cstddef>
#include <functional>

namespace aster {

struct RenderGraphPassInvocation {
  std::size_t index = 0u;
  RenderGraphPass semantic = RenderGraphPass::SceneColorDepth;
  const framegraph::CompiledPass *pass = nullptr;
};

using RenderGraphPassCallback = std::function<void(const RenderGraphPassInvocation &)>;

[[nodiscard]] std::size_t executeFixedRenderGraph(const FixedRenderGraph &graph,
                                                  const RenderGraphPassCallback &callback);

} // namespace aster
