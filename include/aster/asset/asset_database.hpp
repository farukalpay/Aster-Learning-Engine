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

struct AssetSourceLocation {
  std::string source_path;
  std::uint32_t line = 0u;
  std::uint32_t column = 0u;
};

struct AssetSourceRecord {
  std::string path;
  std::string hash;
};

struct AssetImportPresetRecord {
  std::string name;
  std::string origin_policy;
  std::string unit_scale;
  std::string texture_role_policy;
  std::string material_slot_policy;
  std::string collision_policy;
  std::string lod_policy;
  std::string meshlet_policy;
  std::string skeleton_policy;
  std::string animation_policy;
  std::string morph_policy;
};

struct AssetPlatformProfileRecord {
  std::string name;
  std::string runtime_texture_format;
  std::string compression;
  std::string target;
};

struct AssetDependencyRecord {
  std::string role;
  std::string path;
  bool present = false;
  std::string hash;
};

struct AssetDependencyEdge {
  std::string from;
  std::string to;
  std::string role;
  bool present = false;
  std::string hash;
};

struct AssetCookedOutput {
  std::string role;
  std::string kind;
  std::string path;
  std::string hash;
};

struct AssetArtifactRecord {
  std::string role;
  std::string kind;
  std::string path;
  std::string hash;
  bool reused = false;
};

struct AssetCookDiagnostic {
  std::string severity;
  std::string message;
  std::string source_path;
  std::uint32_t line = 0u;
  std::uint32_t column = 0u;
  std::vector<AssetSourceLocation> source_locations;
};

struct AssetToolVersionRecord {
  std::string name;
  std::string version;
};

struct AssetDatabaseRecord {
  std::string guid;
  std::string id;
  std::string kind;
  AssetSourceRecord source;
  std::string source_path;
  AssetImportPresetRecord import_preset;
  AssetPlatformProfileRecord platform_profile;
  std::uint32_t import_settings_version = 0u;
  std::string source_hash;
  std::string options_hash;
  std::vector<AssetDependencyEdge> dependency_edges;
  std::vector<AssetDependencyRecord> dependencies;
  std::vector<AssetArtifactRecord> artifacts;
  std::vector<AssetCookedOutput> outputs;
  std::vector<AssetCookDiagnostic> diagnostics;
  std::vector<AssetToolVersionRecord> tool_versions;
  std::string platform;
};

struct AssetDatabase {
  std::uint32_t schema_version = 0u;
  std::string platform;
  std::string artifact_manifest;
  std::vector<AssetToolVersionRecord> tool_versions;
  std::filesystem::path source_path;
  std::vector<AssetDatabaseRecord> records;
};

struct CookedMaterialTextureRecord {
  std::string role;
  std::filesystem::path source_path;
  std::filesystem::path cooked_path;
  std::string kind;
  std::string color_space;
  std::string source_format;
  std::string runtime_format;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t mip_count = 0u;
  std::uint64_t byte_cost = 0u;
  std::string encoder;
  std::string fallback_reason;
  std::string platform_compatibility;
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
