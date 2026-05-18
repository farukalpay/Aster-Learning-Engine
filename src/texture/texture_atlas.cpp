// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_atlas.hpp"

#include <algorithm>

namespace aster {

TextureAtlasPlan packTextureAtlasRows(const std::vector<TextureAssetMetadata> &textures,
                                      const std::uint32_t max_width) {
  TextureAtlasPlan plan;
  std::uint32_t x = 0u;
  std::uint32_t y = 0u;
  std::uint32_t row_height = 0u;
  plan.width = std::max(max_width, 1u);
  for (const TextureAssetMetadata &texture : textures) {
    const std::uint32_t width = std::max(texture.width, 1u);
    const std::uint32_t height = std::max(texture.height, 1u);
    if (x > 0u && x + width > plan.width) {
      x = 0u;
      y += row_height;
      row_height = 0u;
    }
    plan.entries.push_back({.texture = texture, .x = x, .y = y});
    x += width;
    row_height = std::max(row_height, height);
  }
  plan.height = y + row_height;
  return plan;
}

} // namespace aster
