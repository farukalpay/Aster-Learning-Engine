// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/render/render_graph_executor.hpp"

namespace aster {

std::size_t executeFixedRenderGraph(const FixedRenderGraph &graph,
                                    const RenderGraphPassCallback &callback) {
  if (!graph.valid() || !callback) {
    return 0u;
  }
  std::size_t invoked = 0u;
  for (std::size_t index = 0; index < graph.passes.size(); ++index) {
    const framegraph::CompiledPass &pass = graph.passes[index];
    if (pass.culled) {
      continue;
    }
    const RenderGraphPass semantic = renderGraphPassFromName(pass.name);
    callback({.index = index,
              .semantic = semantic,
              .pass = &pass,
              .declaration = defaultRenderPassDeclaration(semantic)});
    ++invoked;
  }
  return invoked;
}

} // namespace aster
