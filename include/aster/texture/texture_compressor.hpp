// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/texture/texture_asset.hpp"

namespace aster {

struct TextureBakeOptions {
  TextureCompression compression = TextureCompression::Ktx2Basis;
  bool generate_mips = true;
};

struct TextureBakePlan {
  TextureAssetMetadata metadata;
  TextureBakeOptions options;
};

[[nodiscard]] TextureBakePlan planTextureBake(TextureAssetMetadata metadata,
                                             TextureBakeOptions options = {});

} // namespace aster
