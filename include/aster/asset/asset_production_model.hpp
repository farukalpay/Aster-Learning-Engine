// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"
#include "aster/asset/mesh_pipeline.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

struct AssetPreviewImage {
  std::filesystem::path path;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::vector<std::uint8_t> rgba8;
  bool available = false;
  std::string diagnostic;
};

struct AssetProductionTextureAudit {
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
  bool present = false;
  std::vector<std::string> diagnostics;
};

struct AssetProductionMaterialAudit {
  bool attempted = false;
  bool loaded = false;
  std::filesystem::path material_bin_path;
  std::string asset_guid;
  std::string id;
  std::string name;
  std::string shader_variant_tag;
  std::string pipeline_tag;
  std::uint64_t feature_mask = 0u;
  std::uint64_t shader_variant_key = 0u;
  std::vector<std::string> required_roles;
  std::vector<AssetProductionTextureAudit> textures;
  std::vector<std::string> diagnostics;
};

struct AssetProductionMeshAudit {
  bool attempted = false;
  bool loaded = false;
  std::filesystem::path cache_path;
  std::size_t material_count = 0u;
  std::size_t mesh_count = 0u;
  std::size_t collision_mesh_count = 0u;
  std::size_t scene_node_count = 0u;
  std::size_t total_vertices = 0u;
  std::size_t total_indices = 0u;
  std::size_t total_collision_triangles = 0u;
  MeshDiagnostics diagnostics{};
  std::vector<std::string> messages;
};

struct AssetProductionCookAudit {
  std::size_t error_count = 0u;
  std::size_t warning_count = 0u;
  std::vector<AssetCookDiagnostic> diagnostics;
  std::vector<AssetDependencyEdge> dependency_edges;
  std::vector<AssetCookedOutput> outputs;
  AssetDerivedHashes hashes;
  AssetImportPresetRecord import_preset;
  AssetPlatformProfileRecord platform_profile;
};

struct AssetProductionAsset {
  std::string guid;
  std::string id;
  std::string name;
  std::string kind;
  std::filesystem::path source_path;
  bool production_ready = false;
  AssetPreviewImage preview;
  AssetProductionMaterialAudit material;
  AssetProductionMeshAudit mesh;
  std::vector<AssetProductionTextureAudit> textures;
  AssetProductionCookAudit cook;
  std::vector<std::string> model_diagnostics;

  [[nodiscard]] const AssetProductionTextureAudit *
  findTexture(std::string_view role) const noexcept;
  [[nodiscard]] bool hasErrors() const noexcept;
};

class AssetProductionModel {
public:
  std::filesystem::path root_path;
  std::vector<AssetProductionAsset> assets;

  [[nodiscard]] static AssetProductionModel fromDatabase(
      const AssetDatabase &database, const std::filesystem::path &database_root);
  [[nodiscard]] const AssetProductionAsset *find(std::string_view id_or_guid) const noexcept;
};

[[nodiscard]] AssetPreviewImage loadAssetPreviewImage(const std::filesystem::path &path);

} // namespace aster
