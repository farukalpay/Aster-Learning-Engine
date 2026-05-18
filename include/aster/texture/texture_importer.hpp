// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"
#include "aster/texture/texture_asset.hpp"

#include <filesystem>
#include <vector>

namespace aster {

struct TextureImportOptions {
  bool require_existing_files = true;
  bool generate_mips = true;
  TextureCompression compression = TextureCompression::Ktx2Basis;
};

struct TextureSetValidation {
  bool ok = true;
  std::vector<std::string> diagnostics;
  std::vector<TextureAssetMetadata> textures;
};

[[nodiscard]] TextureAssetMetadata inspectTextureAsset(const std::filesystem::path &path,
                                                       TextureKind kind,
                                                       TextureImportOptions options = {});
[[nodiscard]] TextureSetValidation validateMaterialTextureSet(
    const MaterialAsset &asset, const std::filesystem::path &asset_root = {},
    TextureImportOptions options = {});

} // namespace aster
