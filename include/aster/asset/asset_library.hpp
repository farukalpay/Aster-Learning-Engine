// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aster {

struct AssetOperationReport {
  std::filesystem::path path;
  bool ok = true;
  std::size_t warning_count = 0u;
  std::size_t error_count = 0u;
  std::vector<std::string> diagnostics;

  void add(std::string diagnostic);
};

class AssetCatalogPath {
public:
  AssetCatalogPath() = default;
  explicit AssetCatalogPath(const char *path);
  explicit AssetCatalogPath(std::string path);
  explicit AssetCatalogPath(std::string_view path);

  [[nodiscard]] const std::string &str() const noexcept;
  [[nodiscard]] const char *c_str() const noexcept;
  [[nodiscard]] std::size_t length() const noexcept;
  [[nodiscard]] std::string simpleName() const;
  [[nodiscard]] AssetCatalogPath parent() const;
  [[nodiscard]] AssetCatalogPath cleanup() const;
  [[nodiscard]] bool isContainedIn(const AssetCatalogPath &other) const;
  [[nodiscard]] AssetCatalogPath rebase(const AssetCatalogPath &from,
                                        const AssetCatalogPath &to) const;
  [[nodiscard]] std::vector<std::string> components() const;
  [[nodiscard]] explicit operator bool() const noexcept;

  friend bool operator==(const AssetCatalogPath &lhs, const AssetCatalogPath &rhs) noexcept {
    return lhs.path_ == rhs.path_;
  }

  friend bool operator!=(const AssetCatalogPath &lhs, const AssetCatalogPath &rhs) noexcept {
    return !(lhs == rhs);
  }

  friend bool operator<(const AssetCatalogPath &lhs, const AssetCatalogPath &rhs) noexcept {
    return lhs.path_ < rhs.path_;
  }

private:
  std::string path_;
};

struct AssetCatalogRecord {
  std::string id;
  AssetCatalogPath path;
  std::string simple_name;
  std::vector<std::string> tags;
  std::map<std::string, std::string> metadata;
  bool deleted = false;
};

struct AssetCatalogEntry {
  std::string catalog_id;
  std::string catalog_path;
  std::vector<std::size_t> asset_indices;
  std::vector<std::string> tags;
};

enum class AssetLibrarySourceKind {
  OnDisk,
  Runtime,
  Remote,
  Essentials,
};

struct AssetLibrarySourceRecord {
  std::string id;
  AssetLibrarySourceKind kind = AssetLibrarySourceKind::OnDisk;
  std::filesystem::path root_path;
  bool available = true;
  std::vector<std::string> diagnostics;
};

struct AssetCatalogTreeNode {
  std::string name;
  std::string catalog_path;
  std::vector<std::size_t> catalog_indices;
  std::vector<AssetCatalogTreeNode> children;

  [[nodiscard]] AssetCatalogTreeNode *findChild(std::string_view child_name) noexcept;
  [[nodiscard]] const AssetCatalogTreeNode *findChild(std::string_view child_name) const noexcept;
};

struct AssetRepresentation {
  std::string guid;
  std::string id;
  std::string name;
  std::string kind;
  std::string catalog_path;
  std::filesystem::path source_path;
  std::filesystem::path preview_path;
  bool production_ready = false;
  std::uintmax_t source_size_bytes = 0u;
  std::string content_hash;
  AssetDerivedHashes derived_hashes;
  AssetFateReport fate_report;
  std::vector<std::string> diagnostics;
  std::vector<std::string> tags;
  std::vector<std::string> dependency_ids;
  std::vector<std::string> creative_variant_tags;
  std::vector<std::string> variant_intent_tags;
  std::vector<std::string> production_readiness_reasons;
  std::map<std::string, std::string> metadata;

  [[nodiscard]] static AssetRepresentation fromRecord(const AssetDatabaseRecord &record,
                                                      const std::filesystem::path &database_root);
};

struct AssetImportRecipe {
  std::string id;
  std::string guid;
  std::string kind;
  std::filesystem::path source_path;
  AssetCatalogPath catalog_path;
  AssetLibrarySourceKind source_kind = AssetLibrarySourceKind::OnDisk;
  AssetImportPresetRecord import_preset;
  AssetPlatformProfileRecord platform_profile;
  std::vector<std::string> dependency_ids;
  std::vector<std::string> variant_intent_tags;
  std::vector<std::string> production_readiness_reasons;
  std::map<std::string, std::string> metadata;
};

struct AssetFoundryCatalogAudit {
  std::size_t catalog_count = 0u;
  std::size_t asset_count = 0u;
  std::size_t production_ready_assets = 0u;
  std::size_t orphaned_assets = 0u;
  std::size_t duplicate_catalog_paths = 0u;
  std::vector<std::string> variant_intent_tags;
  std::vector<std::string> production_readiness_reasons;
  std::vector<std::string> diagnostics;
};

struct AssetFoundryReport {
  std::filesystem::path root_path;
  std::vector<AssetLibrarySourceRecord> sources;
  std::vector<AssetImportRecipe> import_recipes;
  AssetFoundryCatalogAudit catalog_audit;
  std::size_t dependency_edge_count = 0u;
  std::vector<std::string> diagnostics;
};

struct CookLineageAsset {
  std::string id;
  std::string guid;
  std::string kind;
  std::string source_path;
  bool production_ready = false;
  std::size_t dependency_count = 0u;
  std::size_t output_count = 0u;
  std::size_t diagnostic_count = 0u;
  AssetDerivedHashes hashes;
  std::vector<std::string> chain;
  std::vector<std::string> production_readiness_reasons;
};

struct CookLineageReport {
  std::string platform;
  std::string project_fingerprint;
  std::size_t asset_count = 0u;
  std::size_t production_ready_assets = 0u;
  std::size_t dependency_edge_count = 0u;
  std::size_t output_count = 0u;
  std::vector<CookLineageAsset> assets;
  std::vector<std::string> diagnostics;
};

struct AsterAssetFoundryLineageStep {
  std::string id;
  std::string kind;
  std::string evidence;
  bool ready = false;
};

struct AsterAssetFoundryStory {
  bool production_ready = false;
  std::size_t asset_count = 0u;
  std::size_t production_ready_assets = 0u;
  std::size_t dependency_edge_count = 0u;
  std::size_t preview_artifacts = 0u;
  std::size_t node_preview_records = 0u;
  std::vector<AsterAssetFoundryLineageStep> steps;
  std::vector<std::string> diagnostics;
};

struct AssetFileListEntry {
  std::filesystem::path path;
  std::string kind;
  std::uintmax_t size_bytes = 0u;
  std::string content_hash;
};

struct AssetLibraryManifest {
  std::filesystem::path root_path;
  std::string source_id;
  std::vector<AssetCatalogRecord> catalogs;
  std::vector<AssetFileListEntry> files;
  std::vector<std::string> diagnostics;
};

class DiskFileHashService {
public:
  explicit DiskFileHashService(std::filesystem::path storage_path = {});

  [[nodiscard]] std::string getHash(const std::filesystem::path &path,
                                    std::string_view hash_algorithm = "fnv1a64") const;
  [[nodiscard]] bool fileMatches(const std::filesystem::path &path,
                                 std::string_view hash_algorithm,
                                 std::string_view hex_hash,
                                 std::uintmax_t size_bytes) const;
  [[nodiscard]] const std::filesystem::path &storagePath() const noexcept;

private:
  std::filesystem::path storage_path_;
};

class AssetLibrary {
public:
  std::filesystem::path root_path;
  std::vector<AssetLibrarySourceRecord> sources;
  std::vector<AssetCatalogEntry> catalogs;
  std::vector<AssetCatalogRecord> catalog_records;
  std::vector<AssetRepresentation> assets;
  AssetCatalogTreeNode catalog_tree;
  std::vector<AssetDependencyEdge> dependency_edges;

  [[nodiscard]] static AssetLibrary fromDatabase(const AssetDatabase &database,
                                                 const std::filesystem::path &database_root);
  [[nodiscard]] const AssetRepresentation *find(std::string_view id_or_guid) const;
  [[nodiscard]] std::vector<const AssetRepresentation *> assetsInCatalog(
      std::string_view catalog_path) const;
};

class AssetCatalogStore {
public:
  std::uint32_t schema_version = 1u;
  std::filesystem::path source_path;
  std::vector<AssetCatalogRecord> catalogs;
  AssetOperationReport report;

  [[nodiscard]] const AssetCatalogRecord *findById(std::string_view id) const noexcept;
  [[nodiscard]] const AssetCatalogRecord *findByPath(const AssetCatalogPath &path) const noexcept;
  AssetCatalogRecord &upsert(AssetCatalogRecord record);
  void mergeFrom(const AssetCatalogStore &other);
  [[nodiscard]] AssetCatalogTreeNode buildTree() const;
};

enum class OutlinerDropInsertType {
  Before,
  After,
  Into,
};

struct OutlinerDropTarget {
  std::size_t row = 0u;
  OutlinerDropInsertType insert = OutlinerDropInsertType::Into;
  std::string parent_id;
  bool accepts_payload = false;

  [[nodiscard]] static std::optional<OutlinerDropTarget>
  find(std::size_t row_count, float pointer_y, float row_height, std::string_view hovered_id,
       bool hovered_accepts_children);
};

struct NodePreviewRecord {
  std::string node_id;
  std::uint32_t refresh_state = 0u;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint64_t content_hash = 0u;
  std::vector<std::uint8_t> rgba8;
};

class NodePreviewCache {
public:
  void put(NodePreviewRecord preview);
  [[nodiscard]] const NodePreviewRecord *acquire(std::string_view node_id,
                                                 std::uint32_t refresh_state) const;
  void invalidate(std::string_view node_id);
  void clear();
  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::unordered_map<std::string, NodePreviewRecord> previews_;
};

[[nodiscard]] std::vector<AssetFileListEntry>
scanAssetFiles(const std::filesystem::path &root,
               const DiskFileHashService &hash_service = DiskFileHashService{});
[[nodiscard]] std::string stableAssetCatalogId(const AssetCatalogPath &path);
[[nodiscard]] AssetCatalogStore loadAssetCatalogStore(const std::filesystem::path &path);
[[nodiscard]] AssetOperationReport saveAssetCatalogStore(const AssetCatalogStore &store,
                                                         const std::filesystem::path &path);
[[nodiscard]] AssetCatalogStore makeAssetCatalogStoreFromLibrary(const AssetLibrary &library);
[[nodiscard]] AssetLibraryManifest
buildAssetLibraryManifest(const AssetLibrary &library,
                          const DiskFileHashService &hash_service = DiskFileHashService{});
[[nodiscard]] AssetImportRecipe buildAssetImportRecipe(const AssetRepresentation &asset);
[[nodiscard]] AssetFoundryReport buildAssetFoundryReport(const AssetLibrary &library);
[[nodiscard]] CookLineageReport buildCookLineageReport(const AssetDatabase &database);
[[nodiscard]] AsterAssetFoundryStory buildAsterAssetFoundryStory(
    const AssetLibrary &library, const CookLineageReport &lineage,
    const NodePreviewCache *preview_cache = nullptr);

} // namespace aster
