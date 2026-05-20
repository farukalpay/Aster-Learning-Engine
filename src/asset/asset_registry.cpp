// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_registry.hpp"

#include <algorithm>
#include <set>

namespace aster {

namespace {

bool containsString(const std::vector<std::string> &values, const std::string_view value) {
  return std::any_of(values.begin(), values.end(),
                     [&](const std::string &candidate) { return candidate == value; });
}

bool pathMatches(const std::string_view candidate, const std::string_view query,
                 const bool recursive) {
  if (query.empty()) {
    return true;
  }
  if (candidate == query) {
    return true;
  }
  return recursive && candidate.size() > query.size() && candidate.substr(0u, query.size()) == query &&
         candidate[query.size()] == '/';
}

std::vector<std::string> sortedUnique(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

} // namespace

void AssetRegistry::clear() {
  records_.clear();
  dependency_edges_.clear();
  index_.clear();
}

void AssetRegistry::scanDatabase(const AssetDatabase &database,
                                 const std::filesystem::path &database_root) {
  scanLibrary(AssetLibrary::fromDatabase(database, database_root));
}

void AssetRegistry::scanLibrary(const AssetLibrary &library) {
  records_.clear();
  records_.reserve(library.assets.size());
  for (const AssetRepresentation &asset : library.assets) {
    records_.push_back(makeAssetRegistryRecord(asset));
  }

  std::vector<AssetRegistryDependency> dependencies;
  dependencies.reserve(library.dependency_edges.size());
  for (const AssetDependencyEdge &edge : library.dependency_edges) {
    dependencies.push_back({edge.from, edge.to, edge.role, edge.present, edge.hash});
  }
  setDependencies(std::move(dependencies));
  rebuildIndex();
  changes_.emit({AssetRegistryChangeKind::Scanned, {}, records_.size()});
}

bool AssetRegistry::upsert(AssetRegistryRecord record) {
  if (record.id.empty()) {
    return false;
  }
  const std::string changed_id = record.id;
  const std::optional<std::size_t> existing = indexOf(record.id);
  const AssetRegistryChangeKind kind =
      existing ? AssetRegistryChangeKind::Updated : AssetRegistryChangeKind::Added;
  if (existing) {
    records_[*existing] = std::move(record);
  } else {
    records_.push_back(std::move(record));
  }
  rebuildIndex();
  changes_.emit({kind, changed_id, records_.size()});
  return true;
}

bool AssetRegistry::remove(const std::string_view id_or_guid) {
  const std::optional<std::size_t> index = indexOf(id_or_guid);
  if (!index) {
    return false;
  }
  const std::string removed_id = records_[*index].id;
  records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(*index));
  dependency_edges_.erase(std::remove_if(dependency_edges_.begin(), dependency_edges_.end(),
                                         [&](const AssetRegistryDependency &edge) {
                                           return edge.from == removed_id || edge.to == removed_id;
                                         }),
                          dependency_edges_.end());
  rebuildIndex();
  changes_.emit({AssetRegistryChangeKind::Removed, removed_id, records_.size()});
  return true;
}

const AssetRegistryRecord *AssetRegistry::find(const std::string_view id_or_guid) const {
  const std::optional<std::size_t> index = indexOf(id_or_guid);
  return index ? &records_[*index] : nullptr;
}

std::vector<const AssetRegistryRecord *> AssetRegistry::query(
    const AssetRegistryQuery &asset_query) const {
  std::vector<const AssetRegistryRecord *> result;
  for (const AssetRegistryRecord &record : records_) {
    if (!asset_query.ids.empty() && !containsString(asset_query.ids, record.id) &&
        !containsString(asset_query.ids, record.guid)) {
      continue;
    }
    if (!asset_query.kinds.empty() && !containsString(asset_query.kinds, record.kind)) {
      continue;
    }
    if (!asset_query.catalog_paths.empty() &&
        !std::any_of(asset_query.catalog_paths.begin(), asset_query.catalog_paths.end(),
                     [&](const std::string &catalog_path) {
                       return pathMatches(record.catalog_path, catalog_path,
                                          asset_query.recursive_paths);
                     })) {
      continue;
    }
    if (asset_query.production_ready && record.production_ready != *asset_query.production_ready) {
      continue;
    }
    bool has_tags = true;
    for (const std::string &tag : asset_query.tags) {
      has_tags = has_tags && containsString(record.tags, tag);
    }
    if (!has_tags) {
      continue;
    }
    bool metadata_matches = true;
    for (const auto &[key, value] : asset_query.metadata_equals) {
      const auto found = record.metadata.find(key);
      metadata_matches = metadata_matches && found != record.metadata.end() && found->second == value;
    }
    if (!metadata_matches) {
      continue;
    }
    result.push_back(&record);
  }
  return result;
}

std::vector<const AssetRegistryRecord *> AssetRegistry::assetsByCatalogPath(
    const std::string_view catalog_path, const bool recursive) const {
  return query({.catalog_paths = {std::string(catalog_path)}, .recursive_paths = recursive});
}

std::vector<const AssetRegistryRecord *> AssetRegistry::assetsByKind(
    const std::string_view kind) const {
  return query({.kinds = {std::string(kind)}});
}

std::vector<std::string> AssetRegistry::dependencies(const std::string_view id_or_guid) const {
  const AssetRegistryRecord *record = find(id_or_guid);
  if (!record) {
    return {};
  }
  std::vector<std::string> result = record->dependency_ids;
  for (const AssetRegistryDependency &edge : dependency_edges_) {
    if (edge.from == record->id) {
      result.push_back(edge.to);
    }
  }
  return sortedUnique(std::move(result));
}

std::vector<std::string> AssetRegistry::referencers(const std::string_view id_or_guid) const {
  const AssetRegistryRecord *record = find(id_or_guid);
  const std::string target = record ? record->id : std::string(id_or_guid);
  std::vector<std::string> result;
  for (const AssetRegistryDependency &edge : dependency_edges_) {
    if (edge.to == target) {
      result.push_back(edge.from);
    }
  }
  for (const AssetRegistryRecord &candidate : records_) {
    if (containsString(candidate.dependency_ids, target)) {
      result.push_back(candidate.id);
    }
  }
  return sortedUnique(std::move(result));
}

std::vector<std::string> AssetRegistry::catalogPaths() const {
  std::vector<std::string> result;
  result.reserve(records_.size());
  for (const AssetRegistryRecord &record : records_) {
    if (!record.catalog_path.empty()) {
      result.push_back(record.catalog_path);
    }
  }
  return sortedUnique(std::move(result));
}

const std::vector<AssetRegistryRecord> &AssetRegistry::records() const noexcept {
  return records_;
}

const std::vector<AssetRegistryDependency> &AssetRegistry::dependencyEdges() const noexcept {
  return dependency_edges_;
}

void AssetRegistry::rebuildIndex() {
  index_.clear();
  for (std::size_t index = 0u; index < records_.size(); ++index) {
    if (!records_[index].id.empty()) {
      index_[records_[index].id] = index;
    }
    if (!records_[index].guid.empty()) {
      index_[records_[index].guid] = index;
    }
  }
}

void AssetRegistry::setDependencies(std::vector<AssetRegistryDependency> dependencies) {
  dependency_edges_ = std::move(dependencies);
}

std::optional<std::size_t> AssetRegistry::indexOf(const std::string_view id_or_guid) const {
  const auto found = index_.find(std::string(id_or_guid));
  if (found == index_.end()) {
    return std::nullopt;
  }
  return found->second;
}

AssetRegistryRecord makeAssetRegistryRecord(const AssetRepresentation &asset) {
  AssetRegistryRecord record;
  record.guid = asset.guid;
  record.id = asset.id;
  record.name = asset.name;
  record.kind = asset.kind;
  record.catalog_path = asset.catalog_path;
  record.source_path = asset.source_path;
  record.preview_path = asset.preview_path;
  record.production_ready = asset.production_ready;
  record.source_size_bytes = asset.source_size_bytes;
  record.content_hash = asset.content_hash;
  record.tags = asset.tags;
  record.dependency_ids = asset.dependency_ids;
  record.metadata = asset.metadata;
  return record;
}

const char *assetRegistryChangeKindName(const AssetRegistryChangeKind kind) {
  switch (kind) {
  case AssetRegistryChangeKind::Added:
    return "added";
  case AssetRegistryChangeKind::Updated:
    return "updated";
  case AssetRegistryChangeKind::Removed:
    return "removed";
  case AssetRegistryChangeKind::Scanned:
    return "scanned";
  }
  return "unknown";
}

} // namespace aster
