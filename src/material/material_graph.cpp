// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_graph.hpp"

namespace aster {

MaterialGraph materialGraphForAsset(const MaterialAsset &asset) {
  MaterialGraph graph;
  graph.nodes.reserve(asset.layers.size());
  for (const MaterialLayerExpression &layer : asset.layers) {
    graph.nodes.push_back({.id = layer.name, .operation = layer.operation, .inputs = layer.arguments});
  }
  return graph;
}

} // namespace aster
