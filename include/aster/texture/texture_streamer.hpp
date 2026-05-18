// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/texture/texture_asset.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace aster {

struct TextureStreamingBudget {
  std::size_t byte_budget = 64u * 1024u * 1024u;
};

struct TextureStreamingState {
  TextureStreamingBudget budget{};
  std::map<std::string, TextureAssetMetadata> resident_textures;
};

[[nodiscard]] std::size_t estimatedTextureResidentBytes(const TextureAssetMetadata &metadata);

} // namespace aster
