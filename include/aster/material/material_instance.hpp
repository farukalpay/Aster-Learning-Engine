// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/material/material_parameter_block.hpp"

#include <string>

namespace aster {

struct MaterialInstance {
  std::string id;
  std::string asset_id;
  MaterialParameterBlock overrides;
};

[[nodiscard]] MaterialParameterBlock resolveMaterialParameters(const MaterialAsset &asset,
                                                              const MaterialInstance *instance);

} // namespace aster
