// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct AssetDependencyRecord {
  std::string role;
  std::string path;
  bool present = false;
  std::string hash;
};

struct AssetCookedOutput {
  std::string role;
  std::string kind;
  std::string path;
  std::string hash;
};

struct AssetCookDiagnostic {
  std::string severity;
  std::string message;
};

struct AssetDatabaseRecord {
  std::string guid;
  std::string id;
  std::string kind;
  std::string source_path;
  std::uint32_t import_settings_version = 0u;
  std::string source_hash;
  std::string options_hash;
  std::vector<AssetDependencyRecord> dependencies;
  std::vector<AssetCookedOutput> outputs;
  std::vector<AssetCookDiagnostic> diagnostics;
  std::string platform;
};

struct AssetDatabase {
  std::uint32_t schema_version = 0u;
  std::string platform;
  std::filesystem::path source_path;
  std::vector<AssetDatabaseRecord> records;
};

struct CookedMaterialTextureRecord {
  std::string role;
  std::filesystem::path source_path;
  std::filesystem::path cooked_path;
  std::string kind;
  std::string color_space;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t mip_count = 0u;
  std::string source_hash;
  std::string cooked_hash;
  std::vector<std::string> diagnostics;
};

struct CookedMaterialAsset {
  std::filesystem::path material_bin_path;
  std::string asset_guid;
  std::string shader_variant_tag;
  std::string pipeline_tag;
  std::uint64_t feature_mask = 0u;
  std::uint64_t shader_variant_key = 0u;
  MaterialAsset asset;
  std::vector<CookedMaterialTextureRecord> textures;
  std::vector<AssetDependencyRecord> dependencies;
  std::vector<AssetCookDiagnostic> diagnostics;
};

[[nodiscard]] AssetDatabase loadAssetDatabase(const std::filesystem::path &path);
[[nodiscard]] const AssetDatabaseRecord *findAssetRecord(const AssetDatabase &database,
                                                        std::string_view id);
[[nodiscard]] std::optional<std::filesystem::path>
findAssetOutputPath(const AssetDatabaseRecord &record, const std::filesystem::path &database_root,
                    std::string_view role, std::string_view kind = {});
[[nodiscard]] CookedMaterialAsset loadCookedMaterialAsset(const std::filesystem::path &path);

} // namespace aster
