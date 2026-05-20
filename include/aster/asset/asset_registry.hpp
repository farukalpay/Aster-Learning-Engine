// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/asset/asset_database.hpp"
#include "aster/asset/asset_library.hpp"
#include "aster/core/signal.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class AssetRegistryChangeKind {
  Added,
  Updated,
  Removed,
  Scanned,
};

struct AssetRegistryRecord {
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
  std::vector<std::string> tags;
  std::vector<std::string> dependency_ids;
  std::map<std::string, std::string> metadata;
};

struct AssetRegistryDependency {
  std::string from;
  std::string to;
  std::string role;
  bool present = false;
  std::string hash;
};

struct AssetRegistryQuery {
  std::vector<std::string> ids;
  std::vector<std::string> kinds;
  std::vector<std::string> catalog_paths;
  std::vector<std::string> tags;
  std::map<std::string, std::string> metadata_equals;
  std::optional<bool> production_ready;
  bool recursive_paths = false;
};

struct AssetRegistryChangeEvent {
  AssetRegistryChangeKind kind = AssetRegistryChangeKind::Added;
  std::string asset_id;
  std::size_t asset_count = 0u;
};

class AssetRegistry {
public:
  void clear();
  void scanDatabase(const AssetDatabase &database, const std::filesystem::path &database_root);
  void scanLibrary(const AssetLibrary &library);

  bool upsert(AssetRegistryRecord record);
  bool remove(std::string_view id_or_guid);

  [[nodiscard]] const AssetRegistryRecord *find(std::string_view id_or_guid) const;
  [[nodiscard]] std::vector<const AssetRegistryRecord *> query(
      const AssetRegistryQuery &query) const;
  [[nodiscard]] std::vector<const AssetRegistryRecord *> assetsByCatalogPath(
      std::string_view catalog_path, bool recursive = false) const;
  [[nodiscard]] std::vector<const AssetRegistryRecord *> assetsByKind(std::string_view kind) const;
  [[nodiscard]] std::vector<std::string> dependencies(std::string_view id_or_guid) const;
  [[nodiscard]] std::vector<std::string> referencers(std::string_view id_or_guid) const;
  [[nodiscard]] std::vector<std::string> catalogPaths() const;
  [[nodiscard]] const std::vector<AssetRegistryRecord> &records() const noexcept;
  [[nodiscard]] const std::vector<AssetRegistryDependency> &dependencyEdges() const noexcept;

  [[nodiscard]] Signal<const AssetRegistryChangeEvent &> &changes() {
    return changes_;
  }

private:
  void rebuildIndex();
  void setDependencies(std::vector<AssetRegistryDependency> dependencies);
  [[nodiscard]] std::optional<std::size_t> indexOf(std::string_view id_or_guid) const;

  std::vector<AssetRegistryRecord> records_;
  std::vector<AssetRegistryDependency> dependency_edges_;
  std::map<std::string, std::size_t> index_;
  Signal<const AssetRegistryChangeEvent &> changes_;
};

[[nodiscard]] AssetRegistryRecord makeAssetRegistryRecord(const AssetRepresentation &asset);
[[nodiscard]] const char *assetRegistryChangeKindName(AssetRegistryChangeKind kind);

} // namespace aster
