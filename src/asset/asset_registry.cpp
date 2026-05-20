// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_registry.hpp"

#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>
#include <utility>

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

std::string normalizedFilesystemPath(const std::filesystem::path &path) {
  return path.lexically_normal().generic_string();
}

bool metadataContains(const std::map<std::string, std::string> &metadata,
                      const std::string &key, const std::string &needle) {
  const auto found = metadata.find(key);
  return found != metadata.end() && found->second.find(needle) != std::string::npos;
}

std::string immediateParentPath(const std::string_view path) {
  const std::string normalized = AssetPathIndex::normalize(path);
  const std::size_t slash = normalized.find_last_of('/');
  return slash == std::string::npos ? std::string() : normalized.substr(0u, slash);
}

} // namespace

std::string AssetPathIndex::normalize(const std::string_view path) {
  return AssetCatalogPath(path).cleanup().str();
}

bool AssetPathIndex::addPath(const std::string_view path) {
  return addNormalizedPath(normalize(path), true);
}

bool AssetPathIndex::removePath(const std::string_view path) {
  const std::string normalized = normalize(path);
  if (normalized.empty() || parent_to_children_.find(normalized) == parent_to_children_.end()) {
    return false;
  }

  std::set<std::string> paths_to_remove;
  paths_to_remove.insert(normalized);
  collectSubPaths(normalized, true, paths_to_remove);
  for (const std::string &candidate : paths_to_remove) {
    const auto parent = child_to_parent_.find(candidate);
    if (parent != child_to_parent_.end()) {
      const auto parent_children = parent_to_children_.find(parent->second);
      if (parent_children != parent_to_children_.end()) {
        parent_children->second.erase(candidate);
      }
    }
  }
  for (const std::string &candidate : paths_to_remove) {
    parent_to_children_.erase(candidate);
    child_to_parent_.erase(candidate);
  }

  changes_.emit({AssetPathIndexEventKind::Removed, normalized,
                 immediateParentPath(normalized), parent_to_children_.size()});
  return true;
}

void AssetPathIndex::clear() {
  parent_to_children_.clear();
  child_to_parent_.clear();
}

bool AssetPathIndex::contains(const std::string_view path) const {
  const std::string normalized = normalize(path);
  return !normalized.empty() && parent_to_children_.find(normalized) != parent_to_children_.end();
}

std::vector<std::string> AssetPathIndex::allPaths() const {
  std::vector<std::string> paths;
  paths.reserve(parent_to_children_.size());
  for (const auto &[path, children] : parent_to_children_) {
    (void)children;
    if (!path.empty()) {
      paths.push_back(path);
    }
  }
  return paths;
}

std::vector<std::string> AssetPathIndex::subPaths(const std::string_view base_path,
                                                  const bool recursive) const {
  const std::string normalized = normalize(base_path);
  std::set<std::string> paths;
  collectSubPaths(normalized, recursive, paths);
  return {paths.begin(), paths.end()};
}

std::string AssetPathIndex::parentPath(const std::string_view path) const {
  const std::string normalized = normalize(path);
  const auto found = child_to_parent_.find(normalized);
  return found == child_to_parent_.end() ? immediateParentPath(normalized) : found->second;
}

std::size_t AssetPathIndex::size() const noexcept {
  return parent_to_children_.size();
}

bool AssetPathIndex::addNormalizedPath(const std::string &path, const bool emit_event) {
  if (path.empty()) {
    return false;
  }
  if (parent_to_children_.find(path) != parent_to_children_.end()) {
    return false;
  }

  const std::vector<std::string> components = AssetCatalogPath(path).components();
  std::string parent;
  for (std::size_t i = 0u; i < components.size(); ++i) {
    std::string current;
    for (std::size_t component = 0u; component <= i; ++component) {
      if (!current.empty()) {
        current.push_back('/');
      }
      current += components[component];
    }
    const bool inserted = parent_to_children_.emplace(current, std::set<std::string>{}).second;
    if (!parent.empty()) {
      parent_to_children_[parent].insert(current);
      child_to_parent_[current] = parent;
    }
    if (inserted && emit_event) {
      changes_.emit({AssetPathIndexEventKind::Added, current, parent, parent_to_children_.size()});
    }
    parent = current;
  }
  return true;
}

void AssetPathIndex::collectSubPaths(const std::string &base_path, const bool recursive,
                                     std::set<std::string> &out) const {
  const auto found = parent_to_children_.find(base_path);
  if (found == parent_to_children_.end()) {
    return;
  }
  for (const std::string &child : found->second) {
    out.insert(child);
    if (recursive) {
      collectSubPaths(child, true, out);
    }
  }
}

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
    dependencies.push_back({edge.from, edge.to, edge.role, edge.present, edge.hash,
                            assetDependencyKindFromRole(edge.role)});
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
    if (!asset_query.names.empty() && !containsString(asset_query.names, record.name)) {
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
    if (!asset_query.source_paths.empty() &&
        !std::any_of(asset_query.source_paths.begin(), asset_query.source_paths.end(),
                     [&](const std::filesystem::path &source_path) {
                       return normalizedFilesystemPath(record.source_path) ==
                              normalizedFilesystemPath(source_path);
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
    if (!asset_query.tags_any.empty() &&
        !std::any_of(asset_query.tags_any.begin(), asset_query.tags_any.end(),
                     [&](const std::string &tag) { return containsString(record.tags, tag); })) {
      continue;
    }
    bool metadata_matches = true;
    for (const auto &[key, value] : asset_query.metadata_equals) {
      const auto found = record.metadata.find(key);
      metadata_matches = metadata_matches && found != record.metadata.end() && found->second == value;
    }
    for (const auto &[key, value] : asset_query.metadata_contains) {
      metadata_matches = metadata_matches && metadataContains(record.metadata, key, value);
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

void AssetGatherQueue::push(AssetGatherItem item) {
  items_.push_back(std::move(item));
}

void AssetGatherQueue::append(std::vector<AssetGatherItem> items) {
  items_.insert(items_.end(), std::make_move_iterator(items.begin()),
                std::make_move_iterator(items.end()));
}

AssetGatherItem AssetGatherQueue::pop() {
  if (popped_count_ >= items_.size()) {
    throw std::out_of_range("Asset gather queue is empty.");
  }
  return items_[popped_count_++];
}

void AssetGatherQueue::trim() {
  if (popped_count_ == 0u) {
    return;
  }
  items_.erase(items_.begin(), items_.begin() + static_cast<std::ptrdiff_t>(popped_count_));
  popped_count_ = 0u;
}

void AssetGatherQueue::reset() {
  items_.clear();
  popped_count_ = 0u;
}

void AssetGatherQueue::prioritize(
    const std::function<bool(const AssetGatherItem &)> &predicate) {
  if (!predicate) {
    return;
  }
  std::size_t next_priority = popped_count_;
  for (std::size_t index = popped_count_; index < items_.size(); ++index) {
    if (predicate(items_[index])) {
      std::swap(items_[index], items_[next_priority++]);
    }
  }
  std::stable_sort(items_.begin() + static_cast<std::ptrdiff_t>(popped_count_),
                   items_.begin() + static_cast<std::ptrdiff_t>(next_priority),
                   [](const AssetGatherItem &lhs, const AssetGatherItem &rhs) {
                     return lhs.priority > rhs.priority;
                   });
  std::stable_sort(items_.begin() + static_cast<std::ptrdiff_t>(next_priority), items_.end(),
                   [](const AssetGatherItem &lhs, const AssetGatherItem &rhs) {
                     return lhs.priority > rhs.priority;
                   });
}

const AssetGatherItem &AssetGatherQueue::operator[](const std::size_t index) const {
  if (index >= size()) {
    throw std::out_of_range("Asset gather queue index is out of range.");
  }
  return items_[popped_count_ + index];
}

AssetGatherItem &AssetGatherQueue::operator[](const std::size_t index) {
  if (index >= size()) {
    throw std::out_of_range("Asset gather queue index is out of range.");
  }
  return items_[popped_count_ + index];
}

std::size_t AssetGatherQueue::size() const noexcept {
  return items_.size() - popped_count_;
}

bool AssetGatherQueue::empty() const noexcept {
  return size() == 0u;
}

const char *assetPathIndexEventKindName(const AssetPathIndexEventKind kind) {
  switch (kind) {
  case AssetPathIndexEventKind::Added:
    return "added";
  case AssetPathIndexEventKind::Removed:
    return "removed";
  }
  return "unknown";
}

const char *assetDependencyKindName(const AssetDependencyKind kind) {
  switch (kind) {
  case AssetDependencyKind::Hard:
    return "hard";
  case AssetDependencyKind::Soft:
    return "soft";
  case AssetDependencyKind::SearchableName:
    return "searchable-name";
  case AssetDependencyKind::SoftManage:
    return "soft-manage";
  case AssetDependencyKind::HardManage:
    return "hard-manage";
  case AssetDependencyKind::Unknown:
    return "unknown";
  }
  return "unknown";
}

AssetDependencyKind assetDependencyKindFromRole(const std::string_view role) {
  if (role == "soft" || role == "texture" || role == "preview") {
    return AssetDependencyKind::Soft;
  }
  if (role == "name" || role == "tag" || role == "searchable-name") {
    return AssetDependencyKind::SearchableName;
  }
  if (role == "soft-manage" || role == "catalog") {
    return AssetDependencyKind::SoftManage;
  }
  if (role == "hard-manage" || role == "ownership") {
    return AssetDependencyKind::HardManage;
  }
  if (role.empty() || role == "hard" || role == "material" || role == "scene" ||
      role == "albedo" || role == "normal" || role == "orm") {
    return AssetDependencyKind::Hard;
  }
  return AssetDependencyKind::Unknown;
}

} // namespace aster
