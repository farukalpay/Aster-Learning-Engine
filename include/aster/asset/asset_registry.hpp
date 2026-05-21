// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include "aster/asset/asset_database.hpp"
#include "aster/asset/asset_library.hpp"
#include "aster/core/signal.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
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

enum class AssetPathIndexEventKind {
  Added,
  Removed,
};

struct AssetPathIndexEvent {
  AssetPathIndexEventKind kind = AssetPathIndexEventKind::Added;
  std::string path;
  std::string parent_path;
  std::size_t path_count = 0u;
};

class AssetPathIndex {
public:
  [[nodiscard]] static std::string normalize(std::string_view path);

  bool addPath(std::string_view path);
  bool removePath(std::string_view path);
  void clear();

  [[nodiscard]] bool contains(std::string_view path) const;
  [[nodiscard]] std::vector<std::string> allPaths() const;
  [[nodiscard]] std::vector<std::string> subPaths(std::string_view base_path,
                                                  bool recursive = true) const;
  [[nodiscard]] std::string parentPath(std::string_view path) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] Signal<const AssetPathIndexEvent &> &changes() {
    return changes_;
  }

private:
  bool addNormalizedPath(const std::string &path, bool emit_event);
  void collectSubPaths(const std::string &base_path, bool recursive,
                       std::set<std::string> &out) const;

  std::map<std::string, std::set<std::string>> parent_to_children_;
  std::map<std::string, std::string> child_to_parent_;
  Signal<const AssetPathIndexEvent &> changes_;
};

enum class AssetDependencyKind {
  Hard,
  Soft,
  SearchableName,
  SoftManage,
  HardManage,
  Unknown,
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
  AssetDependencyKind kind = AssetDependencyKind::Hard;
};

struct AssetRegistryQuery {
  std::vector<std::string> ids;
  std::vector<std::string> names;
  std::vector<std::string> kinds;
  std::vector<std::string> catalog_paths;
  std::vector<std::filesystem::path> source_paths;
  std::vector<std::string> tags;
  std::vector<std::string> tags_any;
  std::map<std::string, std::string> metadata_equals;
  std::map<std::string, std::string> metadata_contains;
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

struct AssetGatherItem {
  std::string id;
  std::filesystem::path source_path;
  std::string kind;
  std::string catalog_path;
  int priority = 0;
  std::map<std::string, std::string> metadata;
};

class AssetGatherQueue {
public:
  void push(AssetGatherItem item);
  void append(std::vector<AssetGatherItem> items);
  [[nodiscard]] AssetGatherItem pop();
  void trim();
  void reset();
  void prioritize(const std::function<bool(const AssetGatherItem &)> &predicate);

  [[nodiscard]] const AssetGatherItem &operator[](std::size_t index) const;
  [[nodiscard]] AssetGatherItem &operator[](std::size_t index);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

private:
  std::vector<AssetGatherItem> items_;
  std::size_t popped_count_ = 0u;
};

[[nodiscard]] AssetRegistryRecord makeAssetRegistryRecord(const AssetRepresentation &asset);
[[nodiscard]] const char *assetRegistryChangeKindName(AssetRegistryChangeKind kind);
[[nodiscard]] const char *assetPathIndexEventKindName(AssetPathIndexEventKind kind);
[[nodiscard]] const char *assetDependencyKindName(AssetDependencyKind kind);
[[nodiscard]] AssetDependencyKind assetDependencyKindFromRole(std::string_view role);

} // namespace aster
