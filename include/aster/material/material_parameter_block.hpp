// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/math/vec.hpp"

#include <map>
#include <string>

namespace aster {

struct MaterialParameterBlock {
  std::map<std::string, float> scalars;
  std::map<std::string, Vec3> colors;

  [[nodiscard]] float scalar(std::string_view name, float fallback = 0.0f) const;
  [[nodiscard]] Vec3 color(std::string_view name, Vec3 fallback = {}) const;
};

[[nodiscard]] MaterialParameterBlock materialParameterBlockForAsset(const MaterialAsset &asset);

} // namespace aster
