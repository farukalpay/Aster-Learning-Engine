// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/texture/texture_asset.hpp"

#include <vector>

namespace aster {

struct TextureAtlasEntry {
  TextureAssetMetadata texture;
  std::uint32_t x = 0u;
  std::uint32_t y = 0u;
};

struct TextureAtlasPlan {
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<TextureAtlasEntry> entries;
};

[[nodiscard]] TextureAtlasPlan packTextureAtlasRows(const std::vector<TextureAssetMetadata> &textures,
                                                    std::uint32_t max_width);

} // namespace aster
