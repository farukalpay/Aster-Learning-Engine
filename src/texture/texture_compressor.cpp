// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_compressor.hpp"

#include <algorithm>
#include <utility>

namespace aster {

TextureBakePlan planTextureBake(TextureAssetMetadata metadata, const TextureBakeOptions options) {
  metadata.compression = options.compression;
  if (options.generate_mips && metadata.mips.empty() && metadata.width > 0u && metadata.height > 0u) {
    std::uint32_t width = metadata.width;
    std::uint32_t height = metadata.height;
    const std::uint32_t levels = textureMipCount(width, height);
    for (std::uint32_t level = 0u; level < levels; ++level) {
      metadata.mips.push_back({.width = width, .height = height});
      width = std::max(width / 2u, 1u);
      height = std::max(height / 2u, 1u);
    }
  }
  return {.metadata = std::move(metadata), .options = options};
}

} // namespace aster
