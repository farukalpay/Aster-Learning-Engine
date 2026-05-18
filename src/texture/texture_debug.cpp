// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/texture_debug.hpp"

#include <sstream>

namespace aster {

std::string textureDebugSummary(const TextureAssetMetadata &metadata) {
  std::ostringstream out;
  out << textureKindName(metadata.kind) << " " << metadata.width << "x" << metadata.height
      << " " << textureColorSpaceName(metadata.color_space) << " mips=" << metadata.mips.size()
      << " source=" << metadata.source_path.string();
  if (!metadata.valid) {
    out << " invalid";
  }
  return out.str();
}

} // namespace aster
