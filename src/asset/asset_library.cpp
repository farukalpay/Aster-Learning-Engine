// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_library.hpp"

#include "aster/asset/json_document.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace aster {
namespace {

using asset_json::Value;

[[nodiscard]] std::filesystem::path resolveRelative(const std::filesystem::path &root,
                                                    const std::string &path) {
  if (path.empty()) {
    return {};
  }
  const std::filesystem::path value(path);
  return value.is_absolute() ? value : root / value;
}

[[nodiscard]] std::string lowerExtension(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

[[nodiscard]] std::string kindForPath(const std::filesystem::path &path) {
  const std::string extension = lowerExtension(path);
  if (extension == ".astermat") {
    return "material";
  }
  if (extension == ".astercache" || extension == ".gltf" || extension == ".glb") {
    return "scene";
  }
  if (extension == ".ktx2" || extension == ".png" || extension == ".jpg" ||
      extension == ".jpeg" || extension == ".webp") {
    return "texture";
  }
  if (extension == ".asterdb" || extension == ".json") {
    return "database";
  }
  return "asset";
}

[[nodiscard]] std::string hex64(const std::uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

[[nodiscard]] std::string prefixedHex64(const char *prefix, const std::uint64_t value) {
  std::ostringstream out;
  out << prefix << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

void appendHash(std::uint64_t &hash, const std::string_view value) {
  for (const char c : value) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
}

[[nodiscard]] std::uint64_t fnv1a64File(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return 0u;
  }
  std::uint64_t hash = 1469598103934665603ull;
  std::array<char, 8192> buffer{};
  while (file) {
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = file.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(i)]);
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

[[nodiscard]] std::string trimCopy(const std::string_view value) {
  const auto is_space = [](const unsigned char c) {
    return std::isspace(c) != 0;
  };
  std::size_t first = 0u;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) {
    ++first;
  }
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1u]))) {
    --last;
  }
  return std::string(value.substr(first, last - first));
}

[[nodiscard]] std::string cleanCatalogComponent(const std::string_view value) {
  std::string cleaned = trimCopy(value);
  for (char &c : cleaned) {
    if (c == ':' || c == '\\') {
      c = '-';
    }
  }
  return cleaned;
}

[[nodiscard]] std::vector<std::string> splitCatalogComponents(const std::string_view path) {
  std::vector<std::string> out;
  std::size_t start = 0u;
  while (start <= path.size()) {
    const std::size_t slash = path.find_first_of("/\\", start);
    const std::string raw = std::string(path.substr(start, slash == std::string::npos
                                                               ? std::string::npos
                                                               : slash - start));
    const std::string component = cleanCatalogComponent(raw);
    if (!component.empty() && component != ".") {
      if (component == "..") {
        if (!out.empty()) {
          out.pop_back();
        }
      } else {
        out.push_back(component);
      }
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1u;
  }
  return out;
}

[[nodiscard]] std::string joinCatalogComponents(const std::vector<std::string> &components) {
  std::string out;
  for (const std::string &component : components) {
    if (!out.empty()) {
      out.push_back('/');
    }
    out += component;
  }
  return out;
}

[[nodiscard]] std::string catalogPathFor(const AssetDatabaseRecord &record) {
  if (record.kind.empty()) {
    return "Assets";
  }
  std::string kind = record.kind;
  kind[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(kind[0])));
  return "Assets/" + kind;
}

[[nodiscard]] std::vector<std::string> tagsForRecord(const AssetDatabaseRecord &record) {
  std::vector<std::string> tags;
  if (!record.kind.empty()) {
    tags.push_back(record.kind);
  }
  if (!record.platform.empty()) {
    tags.push_back(record.platform);
  }
  if (!record.import_preset.name.empty()) {
    tags.push_back("preset:" + record.import_preset.name);
  }
  if (record.fate_report.production_ready) {
    tags.push_back("production-ready");
  }
  for (const AssetCookedOutput &output : record.outputs) {
    if (!output.role.empty()) {
      tags.push_back("output:" + output.role);
    }
  }
  std::sort(tags.begin(), tags.end());
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

[[nodiscard]] std::vector<std::string>
creativeVariantTagsForRecord(const AssetDatabaseRecord &record) {
  std::vector<std::string> tags;
  const auto add_if_present = [&](const std::string &value, const char *prefix) {
    if (!value.empty() && value != "default") {
      tags.push_back(std::string(prefix) + value);
    }
  };
  add_if_present(record.import_preset.collision_policy, "collision:");
  add_if_present(record.import_preset.lod_policy, "lod:");
  add_if_present(record.import_preset.texture_role_policy, "texture-policy:");
  add_if_present(record.import_preset.material_slot_policy, "material-slots:");
  if (!record.derived_hashes.material_hash.empty()) {
    tags.push_back("material-variant");
  }
  if (!record.derived_hashes.pipeline_cache_key.empty()) {
    tags.push_back("pipeline-variant");
  }
  std::sort(tags.begin(), tags.end());
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

[[nodiscard]] std::map<std::string, std::string>
metadataForRecord(const AssetDatabaseRecord &record) {
  std::map<std::string, std::string> metadata;
  metadata["guid"] = record.guid;
  metadata["kind"] = record.kind;
  metadata["source_path"] = record.source_path;
  metadata["platform"] = record.platform;
  if (!record.import_preset.name.empty()) {
    metadata["import_preset"] = record.import_preset.name;
  }
  if (!record.derived_hashes.source_hash.empty()) {
    metadata["source_hash"] = record.derived_hashes.source_hash;
  } else if (!record.source_hash.empty()) {
    metadata["source_hash"] = record.source_hash;
  }
  if (!record.derived_hashes.artifact_hash.empty()) {
    metadata["artifact_hash"] = record.derived_hashes.artifact_hash;
  }
  metadata["production_ready"] = record.fate_report.production_ready ? "true" : "false";
  metadata["dependencies"] = std::to_string(record.dependency_edges.size() +
                                            record.dependencies.size());
  metadata["outputs"] = std::to_string(record.outputs.size());
  return metadata;
}

[[nodiscard]] std::vector<std::string> stringsFromArray(const Value &json,
                                                        const std::string_view key) {
  std::vector<std::string> out;
  const Value *array = json.find(key);
  if (array == nullptr || array->kind != Value::Kind::Array) {
    return out;
  }
  for (const Value &value : array->array) {
    if (value.kind == Value::Kind::String) {
      out.push_back(value.string);
    }
  }
  return out;
}

[[nodiscard]] std::map<std::string, std::string> metadataFromObject(const Value &json) {
  std::map<std::string, std::string> out;
  const Value *metadata = json.find("metadata");
  if (metadata == nullptr || metadata->kind != Value::Kind::Object) {
    return out;
  }
  for (const auto &[key, value] : metadata->object) {
    if (value.kind == Value::Kind::String) {
      out[key] = value.string;
    } else if (value.kind == Value::Kind::Number) {
      out[key] = std::to_string(value.number);
    } else if (value.kind == Value::Kind::Bool) {
      out[key] = value.boolean ? "true" : "false";
    }
  }
  return out;
}

[[nodiscard]] std::string escapeJson(const std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8u);
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

void addCatalogPath(AssetCatalogTreeNode &root, const std::string &path,
                    const std::size_t catalog_index) {
  AssetCatalogTreeNode *node = &root;
  std::size_t start = 0u;
  while (start < path.size()) {
    const std::size_t slash = path.find('/', start);
    const std::string segment = path.substr(start, slash == std::string::npos
                                                       ? std::string::npos
                                                       : slash - start);
    if (!segment.empty()) {
      AssetCatalogTreeNode *child = node->findChild(segment);
      if (child == nullptr) {
        AssetCatalogTreeNode next;
        next.name = segment;
        next.catalog_path = node->catalog_path.empty() ? segment : node->catalog_path + "/" + segment;
        node->children.push_back(std::move(next));
        child = &node->children.back();
      }
      node = child;
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1u;
  }
  node->catalog_indices.push_back(catalog_index);
}

} // namespace

void AssetOperationReport::add(std::string diagnostic) {
  if (diagnostic.rfind("error:", 0u) == 0u) {
    ++error_count;
    ok = false;
  } else if (diagnostic.rfind("warning:", 0u) == 0u) {
    ++warning_count;
  }
  diagnostics.push_back(std::move(diagnostic));
}

AssetCatalogPath::AssetCatalogPath(std::string path) : path_(std::move(path)) {}

AssetCatalogPath::AssetCatalogPath(const char *path) : path_(path == nullptr ? "" : path) {}

AssetCatalogPath::AssetCatalogPath(const std::string_view path) : path_(path) {}

const std::string &AssetCatalogPath::str() const noexcept {
  return path_;
}

const char *AssetCatalogPath::c_str() const noexcept {
  return path_.c_str();
}

std::size_t AssetCatalogPath::length() const noexcept {
  return path_.size();
}

std::string AssetCatalogPath::simpleName() const {
  const AssetCatalogPath cleaned = cleanup();
  const std::vector<std::string> values = cleaned.components();
  return values.empty() ? std::string() : values.back();
}

AssetCatalogPath AssetCatalogPath::parent() const {
  std::vector<std::string> values = cleanup().components();
  if (!values.empty()) {
    values.pop_back();
  }
  return AssetCatalogPath(joinCatalogComponents(values));
}

AssetCatalogPath AssetCatalogPath::cleanup() const {
  return AssetCatalogPath(joinCatalogComponents(splitCatalogComponents(path_)));
}

bool AssetCatalogPath::isContainedIn(const AssetCatalogPath &other) const {
  const std::string self = cleanup().str();
  const std::string parent_path = other.cleanup().str();
  if (parent_path.empty() || self == parent_path) {
    return true;
  }
  return self.size() > parent_path.size() && self.rfind(parent_path, 0u) == 0u &&
         self[parent_path.size()] == '/';
}

AssetCatalogPath AssetCatalogPath::rebase(const AssetCatalogPath &from,
                                          const AssetCatalogPath &to) const {
  const std::string self = cleanup().str();
  const std::string from_path = from.cleanup().str();
  const std::string to_path = to.cleanup().str();
  if (from_path.empty()) {
    return AssetCatalogPath(to_path.empty() ? self : to_path + "/" + self).cleanup();
  }
  if (!isContainedIn(from)) {
    return {};
  }
  if (self == from_path) {
    return AssetCatalogPath(to_path).cleanup();
  }
  const std::string suffix = self.substr(from_path.size() + 1u);
  return AssetCatalogPath(to_path.empty() ? suffix : to_path + "/" + suffix).cleanup();
}

std::vector<std::string> AssetCatalogPath::components() const {
  return splitCatalogComponents(path_);
}

AssetCatalogPath::operator bool() const noexcept {
  return !path_.empty();
}

AssetCatalogTreeNode *AssetCatalogTreeNode::findChild(
    const std::string_view child_name) noexcept {
  const auto found =
      std::find_if(children.begin(), children.end(), [&](const AssetCatalogTreeNode &child) {
        return child.name == child_name;
      });
  return found == children.end() ? nullptr : &*found;
}

const AssetCatalogTreeNode *AssetCatalogTreeNode::findChild(
    const std::string_view child_name) const noexcept {
  const auto found =
      std::find_if(children.begin(), children.end(), [&](const AssetCatalogTreeNode &child) {
        return child.name == child_name;
      });
  return found == children.end() ? nullptr : &*found;
}

AssetRepresentation AssetRepresentation::fromRecord(const AssetDatabaseRecord &record,
                                                    const std::filesystem::path &database_root) {
  AssetRepresentation representation;
  representation.guid = record.guid;
  representation.id = record.id;
  representation.name = record.id.empty() ? record.source_path : record.id;
  representation.kind = record.kind;
  representation.catalog_path = catalogPathFor(record);
  representation.source_path = resolveRelative(database_root, record.source_path);
  representation.production_ready = record.fate_report.production_ready;
  std::error_code error;
  if (std::filesystem::is_regular_file(representation.source_path, error)) {
    representation.source_size_bytes = std::filesystem::file_size(representation.source_path, error);
  }
  representation.content_hash = !record.derived_hashes.source_hash.empty()
                                    ? record.derived_hashes.source_hash
                                    : (!record.source.hash.empty() ? record.source.hash
                                                                   : record.source_hash);
  representation.derived_hashes = record.derived_hashes;
  representation.fate_report = record.fate_report;
  representation.tags = tagsForRecord(record);
  representation.creative_variant_tags = creativeVariantTagsForRecord(record);
  representation.metadata = metadataForRecord(record);
  for (const AssetDependencyEdge &edge : record.dependency_edges) {
    if (!edge.to.empty()) {
      representation.dependency_ids.push_back(edge.to);
    }
  }
  for (const AssetDependencyRecord &dependency : record.dependencies) {
    if (!dependency.path.empty()) {
      representation.dependency_ids.push_back(dependency.path);
    }
  }
  std::sort(representation.dependency_ids.begin(), representation.dependency_ids.end());
  representation.dependency_ids.erase(
      std::unique(representation.dependency_ids.begin(), representation.dependency_ids.end()),
      representation.dependency_ids.end());
  for (const AssetCookedOutput &output : record.outputs) {
    if (output.role == "preview") {
      representation.preview_path = resolveRelative(database_root, output.path);
      break;
    }
  }
  for (const AssetCookDiagnostic &diagnostic : record.diagnostics) {
    representation.diagnostics.push_back(diagnostic.severity + ": " + diagnostic.message);
  }
  return representation;
}

DiskFileHashService::DiskFileHashService(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

std::string DiskFileHashService::getHash(const std::filesystem::path &path,
                                         const std::string_view hash_algorithm) const {
  if (hash_algorithm != "fnv1a64" && hash_algorithm != "content") {
    return {};
  }
  return hex64(fnv1a64File(path));
}

bool DiskFileHashService::fileMatches(const std::filesystem::path &path,
                                      const std::string_view hash_algorithm,
                                      const std::string_view hex_hash,
                                      const std::uintmax_t size_bytes) const {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) {
    return false;
  }
  if (std::filesystem::file_size(path, error) != size_bytes || error) {
    return false;
  }
  return getHash(path, hash_algorithm) == hex_hash;
}

const std::filesystem::path &DiskFileHashService::storagePath() const noexcept {
  return storage_path_;
}

AssetLibrary AssetLibrary::fromDatabase(const AssetDatabase &database,
                                        const std::filesystem::path &database_root) {
  AssetLibrary library;
  library.root_path = database_root;
  library.sources.push_back({.id = database.source_path.empty() ? std::string("database")
                                                                : database.source_path.stem().string(),
                             .kind = AssetLibrarySourceKind::OnDisk,
                             .root_path = database_root,
                             .available = true});
  library.catalog_tree.name = "Assets";
  library.catalog_tree.catalog_path = "Assets";
  library.dependency_edges.reserve(database.asset_graph.edges.size());
  for (const AssetGraphEdge &edge : database.asset_graph.edges) {
    library.dependency_edges.push_back({.from = edge.from,
                                        .to = edge.to,
                                        .role = edge.role,
                                        .present = edge.present,
                                        .hash = edge.hash});
  }
  std::unordered_map<std::string, std::size_t> catalog_index;
  for (const AssetDatabaseRecord &record : database.records) {
    const std::size_t asset_index = library.assets.size();
    library.assets.push_back(AssetRepresentation::fromRecord(record, database_root));
    const std::string catalog_path = catalogPathFor(record);
    const auto [it, inserted] = catalog_index.emplace(catalog_path, library.catalogs.size());
    if (inserted) {
      library.catalogs.push_back({.catalog_id = stableAssetCatalogId(AssetCatalogPath(catalog_path)),
                                  .catalog_path = catalog_path,
                                  .tags = tagsForRecord(record)});
      AssetCatalogRecord catalog_record;
      catalog_record.id = library.catalogs.back().catalog_id;
      catalog_record.path = AssetCatalogPath(catalog_path).cleanup();
      catalog_record.simple_name = catalog_record.path.simpleName();
      catalog_record.tags = tagsForRecord(record);
      catalog_record.metadata["source"] = "asset-database";
      catalog_record.metadata["kind"] = record.kind;
      library.catalog_records.push_back(std::move(catalog_record));
      addCatalogPath(library.catalog_tree, catalog_path, library.catalogs.size() - 1u);
    } else {
      AssetCatalogEntry &catalog = library.catalogs[it->second];
      std::vector<std::string> tags = tagsForRecord(record);
      catalog.tags.insert(catalog.tags.end(), tags.begin(), tags.end());
      std::sort(catalog.tags.begin(), catalog.tags.end());
      catalog.tags.erase(std::unique(catalog.tags.begin(), catalog.tags.end()),
                         catalog.tags.end());
      if (it->second < library.catalog_records.size()) {
        AssetCatalogRecord &catalog_record = library.catalog_records[it->second];
        catalog_record.tags.insert(catalog_record.tags.end(), tags.begin(), tags.end());
        std::sort(catalog_record.tags.begin(), catalog_record.tags.end());
        catalog_record.tags.erase(
            std::unique(catalog_record.tags.begin(), catalog_record.tags.end()),
            catalog_record.tags.end());
      }
    }
    library.catalogs[it->second].asset_indices.push_back(asset_index);
    if (it->second < library.catalog_records.size()) {
      library.catalog_records[it->second].metadata["asset_count"] =
          std::to_string(library.catalogs[it->second].asset_indices.size());
    }
  }
  return library;
}

const AssetRepresentation *AssetLibrary::find(const std::string_view id_or_guid) const {
  const auto found = std::find_if(assets.begin(), assets.end(),
                                  [&](const AssetRepresentation &asset) {
                                    return asset.id == id_or_guid || asset.guid == id_or_guid;
                                  });
  return found == assets.end() ? nullptr : &*found;
}

std::vector<const AssetRepresentation *> AssetLibrary::assetsInCatalog(
    const std::string_view catalog_path) const {
  std::vector<const AssetRepresentation *> out;
  const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                  [&](const AssetCatalogEntry &catalog) {
                                    return catalog.catalog_path == catalog_path;
                                  });
  if (found == catalogs.end()) {
    return out;
  }
  out.reserve(found->asset_indices.size());
  for (const std::size_t index : found->asset_indices) {
    if (index < assets.size()) {
      out.push_back(&assets[index]);
    }
  }
  return out;
}

const AssetCatalogRecord *AssetCatalogStore::findById(const std::string_view id) const noexcept {
  const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                  [&](const AssetCatalogRecord &catalog) {
                                    return catalog.id == id && !catalog.deleted;
                                  });
  return found == catalogs.end() ? nullptr : &*found;
}

const AssetCatalogRecord *AssetCatalogStore::findByPath(
    const AssetCatalogPath &path) const noexcept {
  const AssetCatalogPath clean = path.cleanup();
  const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                  [&](const AssetCatalogRecord &catalog) {
                                    return catalog.path.cleanup() == clean && !catalog.deleted;
                                  });
  return found == catalogs.end() ? nullptr : &*found;
}

AssetCatalogRecord &AssetCatalogStore::upsert(AssetCatalogRecord record) {
  record.path = record.path.cleanup();
  if (record.simple_name.empty()) {
    record.simple_name = record.path.simpleName();
  }
  if (record.id.empty()) {
    record.id = stableAssetCatalogId(record.path);
  }
  const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                  [&](const AssetCatalogRecord &catalog) {
                                    return catalog.id == record.id ||
                                           catalog.path.cleanup() == record.path;
                                  });
  if (found != catalogs.end()) {
    *found = std::move(record);
    return *found;
  }
  catalogs.push_back(std::move(record));
  return catalogs.back();
}

void AssetCatalogStore::mergeFrom(const AssetCatalogStore &other) {
  for (AssetCatalogRecord record : other.catalogs) {
    upsert(std::move(record));
  }
}

AssetCatalogTreeNode AssetCatalogStore::buildTree() const {
  AssetCatalogTreeNode root;
  root.name = "Assets";
  root.catalog_path = "Assets";
  for (std::size_t i = 0u; i < catalogs.size(); ++i) {
    if (!catalogs[i].deleted) {
      addCatalogPath(root, catalogs[i].path.cleanup().str(), i);
    }
  }
  return root;
}

std::optional<OutlinerDropTarget>
OutlinerDropTarget::find(const std::size_t row_count, const float pointer_y,
                         const float row_height, const std::string_view hovered_id,
                         const bool hovered_accepts_children) {
  if (row_count == 0u || row_height <= 0.0f || pointer_y < 0.0f) {
    return std::nullopt;
  }
  const std::size_t row = std::min<std::size_t>(
      static_cast<std::size_t>(pointer_y / row_height), row_count - 1u);
  const float row_top = static_cast<float>(row) * row_height;
  const float local_y = pointer_y - row_top;
  OutlinerDropTarget target;
  target.row = row;
  target.parent_id = std::string(hovered_id);
  target.accepts_payload = true;
  if (hovered_accepts_children && local_y > row_height * 0.28f && local_y < row_height * 0.72f) {
    target.insert = OutlinerDropInsertType::Into;
  } else if (local_y < row_height * 0.5f) {
    target.insert = OutlinerDropInsertType::Before;
  } else {
    target.insert = OutlinerDropInsertType::After;
  }
  return target;
}

void NodePreviewCache::put(NodePreviewRecord preview) {
  previews_[preview.node_id] = std::move(preview);
}

const NodePreviewRecord *NodePreviewCache::acquire(const std::string_view node_id,
                                                  const std::uint32_t refresh_state) const {
  const auto found = previews_.find(std::string(node_id));
  if (found == previews_.end() || found->second.refresh_state != refresh_state) {
    return nullptr;
  }
  return &found->second;
}

void NodePreviewCache::invalidate(const std::string_view node_id) {
  previews_.erase(std::string(node_id));
}

void NodePreviewCache::clear() {
  previews_.clear();
}

std::size_t NodePreviewCache::size() const noexcept {
  return previews_.size();
}

std::vector<AssetFileListEntry>
scanAssetFiles(const std::filesystem::path &root, const DiskFileHashService &hash_service) {
  std::vector<AssetFileListEntry> entries;
  std::error_code error;
  if (!std::filesystem::exists(root, error)) {
    return entries;
  }
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  for (std::filesystem::recursive_directory_iterator it(root, options, error), end;
       it != end && !error; it.increment(error)) {
    if (!it->is_regular_file(error)) {
      continue;
    }
    AssetFileListEntry entry;
    entry.path = it->path();
    entry.kind = kindForPath(entry.path);
    entry.size_bytes = it->file_size(error);
    entry.content_hash = hash_service.getHash(entry.path);
    entries.push_back(std::move(entry));
  }
  std::sort(entries.begin(), entries.end(), [](const AssetFileListEntry &lhs,
                                               const AssetFileListEntry &rhs) {
    return lhs.path.generic_string() < rhs.path.generic_string();
  });
  return entries;
}

std::string stableAssetCatalogId(const AssetCatalogPath &path) {
  std::uint64_t hash = 1469598103934665603ull;
  appendHash(hash, path.cleanup().str());
  return prefixedHex64("aster-catalog-0x", hash);
}

AssetCatalogStore loadAssetCatalogStore(const std::filesystem::path &path) {
  AssetCatalogStore store;
  store.source_path = path;
  store.report.path = path;
  const Value root = asset_json::parseFile(path);
  store.schema_version = asset_json::u32Or(root, "schema_version", 1u);
  const Value *catalogs = root.find("catalogs");
  if (catalogs == nullptr || catalogs->kind != Value::Kind::Array) {
    store.report.add("warning: catalog store has no catalogs array");
    return store;
  }
  for (const Value &value : catalogs->array) {
    AssetCatalogRecord record;
    record.id = asset_json::textOr(value, "id");
    record.path = AssetCatalogPath(asset_json::textOr(value, "path")).cleanup();
    record.simple_name = asset_json::textOr(value, "simple_name", record.path.simpleName());
    record.tags = stringsFromArray(value, "tags");
    record.metadata = metadataFromObject(value);
    record.deleted = asset_json::boolOr(value, "deleted");
    store.upsert(std::move(record));
  }
  return store;
}

AssetOperationReport saveAssetCatalogStore(const AssetCatalogStore &store,
                                           const std::filesystem::path &path) {
  AssetOperationReport report;
  report.path = path;
  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      report.add("error: could not create catalog directory: " + error.message());
      return report;
    }
  }
  std::ofstream out(path);
  if (!out) {
    report.add("error: could not open catalog store for writing");
    return report;
  }
  out << "{\n  \"schema_version\": " << store.schema_version << ",\n  \"catalogs\": [\n";
  for (std::size_t i = 0u; i < store.catalogs.size(); ++i) {
    const AssetCatalogRecord &catalog = store.catalogs[i];
    out << "    {\n";
    out << "      \"id\": \"" << escapeJson(catalog.id) << "\",\n";
    out << "      \"path\": \"" << escapeJson(catalog.path.cleanup().str()) << "\",\n";
    out << "      \"simple_name\": \"" << escapeJson(catalog.simple_name) << "\",\n";
    out << "      \"tags\": [";
    for (std::size_t tag = 0u; tag < catalog.tags.size(); ++tag) {
      out << (tag == 0u ? "" : ", ") << "\"" << escapeJson(catalog.tags[tag]) << "\"";
    }
    out << "],\n";
    out << "      \"metadata\": {";
    std::size_t written = 0u;
    for (const auto &[key, value] : catalog.metadata) {
      out << (written == 0u ? "" : ", ") << "\"" << escapeJson(key) << "\": \""
          << escapeJson(value) << "\"";
      ++written;
    }
    out << "},\n";
    out << "      \"deleted\": " << (catalog.deleted ? "true" : "false") << "\n";
    out << "    }" << (i + 1u == store.catalogs.size() ? "\n" : ",\n");
  }
  out << "  ]\n}\n";
  if (!out.good()) {
    report.add("error: catalog store write failed");
  }
  return report;
}

AssetCatalogStore makeAssetCatalogStoreFromLibrary(const AssetLibrary &library) {
  AssetCatalogStore store;
  store.source_path = library.root_path / "aster_catalogs.json";
  if (!library.catalog_records.empty()) {
    for (AssetCatalogRecord record : library.catalog_records) {
      store.upsert(std::move(record));
    }
    return store;
  }
  for (const AssetCatalogEntry &entry : library.catalogs) {
    AssetCatalogRecord record;
    record.id = entry.catalog_id.empty() ? stableAssetCatalogId(AssetCatalogPath(entry.catalog_path))
                                         : entry.catalog_id;
    record.path = AssetCatalogPath(entry.catalog_path).cleanup();
    record.simple_name = record.path.simpleName();
    record.tags = entry.tags;
    record.metadata["asset_count"] = std::to_string(entry.asset_indices.size());
    record.metadata["source"] = "asset-library";
    store.upsert(std::move(record));
  }
  return store;
}

AssetLibraryManifest buildAssetLibraryManifest(const AssetLibrary &library,
                                               const DiskFileHashService &hash_service) {
  AssetLibraryManifest manifest;
  manifest.root_path = library.root_path;
  manifest.source_id = library.sources.empty() ? std::string("library")
                                               : library.sources.front().id;
  manifest.catalogs = makeAssetCatalogStoreFromLibrary(library).catalogs;
  manifest.files = scanAssetFiles(library.root_path, hash_service);
  for (const AssetLibrarySourceRecord &source : library.sources) {
    for (const std::string &diagnostic : source.diagnostics) {
      manifest.diagnostics.push_back(diagnostic);
    }
  }
  return manifest;
}

} // namespace aster
