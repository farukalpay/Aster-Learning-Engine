// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_streamer.hpp"

#include <algorithm>

namespace aster {

std::size_t estimatedTextureResidentBytes(const TextureAssetMetadata &metadata) {
  std::size_t bytes = 0u;
  if (metadata.mips.empty()) {
    return static_cast<std::size_t>(std::max(metadata.width, 1u)) *
           static_cast<std::size_t>(std::max(metadata.height, 1u)) * 4u;
  }
  for (const TextureMipInfo &mip : metadata.mips) {
    bytes += static_cast<std::size_t>(std::max(mip.width, 1u)) *
             static_cast<std::size_t>(std::max(mip.height, 1u)) * 4u;
  }
  return bytes;
}

} // namespace aster
