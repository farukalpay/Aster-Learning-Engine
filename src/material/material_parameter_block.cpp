// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_parameter_block.hpp"

namespace aster {

float MaterialParameterBlock::scalar(const std::string_view name, const float fallback) const {
  const auto it = scalars.find(std::string(name));
  return it == scalars.end() ? fallback : it->second;
}

Vec3 MaterialParameterBlock::color(const std::string_view name, const Vec3 fallback) const {
  const auto it = colors.find(std::string(name));
  return it == colors.end() ? fallback : it->second;
}

MaterialParameterBlock materialParameterBlockForAsset(const MaterialAsset &asset) {
  MaterialParameterBlock block;
  block.scalars = asset.params;
  block.colors["base_color"] = {block.scalar("base_color_r", 1.0f),
                                 block.scalar("base_color_g", 1.0f),
                                 block.scalar("base_color_b", 1.0f)};
  block.colors["emission"] = {block.scalar("emission_r", 0.0f),
                               block.scalar("emission_g", 0.0f),
                               block.scalar("emission_b", 0.0f)};
  return block;
}

} // namespace aster
