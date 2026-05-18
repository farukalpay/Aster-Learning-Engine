// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"

#include <string>
#include <vector>

namespace aster {

struct MaterialGraphNode {
  std::string id;
  std::string operation;
  std::vector<std::string> inputs;
};

struct MaterialGraph {
  std::vector<MaterialGraphNode> nodes;
};

[[nodiscard]] MaterialGraph materialGraphForAsset(const MaterialAsset &asset);

} // namespace aster
