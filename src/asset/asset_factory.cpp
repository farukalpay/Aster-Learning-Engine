// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_factory.hpp"

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

[[nodiscard]] std::string catalogPathFor(const AssetDatabaseRecord &record) {
  if (record.kind.empty()) {
    return "Assets";
  }
  std::string kind = record.kind;
  kind[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(kind[0])));
  return "Assets/" + kind;
}

} // namespace

AssetRepresentation AssetRepresentation::fromRecord(const AssetDatabaseRecord &record,
                                                    const std::filesystem::path &database_root) {
  AssetRepresentation representation;
  representation.guid = record.guid;
  representation.id = record.id;
  representation.name = record.id.empty() ? record.source_path : record.id;
  representation.kind = record.kind;
  representation.source_path = resolveRelative(database_root, record.source_path);
  representation.production_ready = record.fate_report.production_ready;
  representation.derived_hashes = record.derived_hashes;
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
  std::unordered_map<std::string, std::size_t> catalog_index;
  for (const AssetDatabaseRecord &record : database.records) {
    const std::size_t asset_index = library.assets.size();
    library.assets.push_back(AssetRepresentation::fromRecord(record, database_root));
    const std::string catalog_path = catalogPathFor(record);
    const auto [it, inserted] = catalog_index.emplace(catalog_path, library.catalogs.size());
    if (inserted) {
      library.catalogs.push_back({.catalog_path = catalog_path});
    }
    library.catalogs[it->second].asset_indices.push_back(asset_index);
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

} // namespace aster
