// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aster {

struct AssetCatalogEntry {
  std::string catalog_path;
  std::vector<std::size_t> asset_indices;
};

struct AssetRepresentation {
  std::string guid;
  std::string id;
  std::string name;
  std::string kind;
  std::filesystem::path source_path;
  std::filesystem::path preview_path;
  bool production_ready = false;
  AssetDerivedHashes derived_hashes;
  std::vector<std::string> diagnostics;

  [[nodiscard]] static AssetRepresentation fromRecord(const AssetDatabaseRecord &record,
                                                      const std::filesystem::path &database_root);
};

struct AssetFileListEntry {
  std::filesystem::path path;
  std::string kind;
  std::uintmax_t size_bytes = 0u;
  std::string content_hash;
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
  std::vector<AssetCatalogEntry> catalogs;
  std::vector<AssetRepresentation> assets;

  [[nodiscard]] static AssetLibrary fromDatabase(const AssetDatabase &database,
                                                 const std::filesystem::path &database_root);
  [[nodiscard]] const AssetRepresentation *find(std::string_view id_or_guid) const;
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

} // namespace aster
