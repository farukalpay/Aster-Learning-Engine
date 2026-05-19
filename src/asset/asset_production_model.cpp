// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_production_model.hpp"

#include "aster/asset/json_document.hpp"
#include "aster/asset/scene_asset_importer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace aster {
namespace {

constexpr std::array<std::string_view, 3> kRequiredLitPbrRoles{"albedo", "normal", "orm"};

[[nodiscard]] std::filesystem::path resolveRelative(const std::filesystem::path &root,
                                                    const std::filesystem::path &path) {
  return path.is_absolute() ? path : root / path;
}

[[nodiscard]] bool pathExists(const std::filesystem::path &path) {
  std::error_code error;
  return std::filesystem::exists(path, error);
}

void appendMissingFileDiagnostic(const std::filesystem::path &path,
                                 std::vector<std::string> &diagnostics) {
  diagnostics.push_back("error: missing output file: " + path.generic_string());
}

[[nodiscard]] bool hasSeverity(const AssetProductionTextureAudit &texture,
                               const std::string_view severity) {
  const std::string prefix = std::string(severity) + ":";
  return std::any_of(texture.diagnostics.begin(), texture.diagnostics.end(),
                     [&](const std::string &diagnostic) {
                       return diagnostic.rfind(prefix, 0u) == 0u;
                     });
}

[[nodiscard]] std::optional<std::filesystem::path>
requiredOutputPath(const AssetDatabaseRecord &record, const std::filesystem::path &database_root,
                   const std::string_view role, const std::string_view kind,
                   const std::string_view label,
                   std::vector<std::string> &diagnostics) {
  std::optional<std::filesystem::path> path =
      findAssetOutputPath(record, database_root, role, kind);
  if (!path.has_value()) {
    diagnostics.push_back("error: missing " + std::string(label) + " output");
    return std::nullopt;
  }
  if (!pathExists(*path)) {
    appendMissingFileDiagnostic(*path, diagnostics);
  }
  return path;
}

[[nodiscard]] std::vector<std::string> stringsFromArray(const asset_json::Value &json,
                                                        const std::string_view key) {
  std::vector<std::string> values;
  const asset_json::Value *array = json.find(key);
  if (array == nullptr || array->kind != asset_json::Value::Kind::Array) {
    return values;
  }
  for (const asset_json::Value &value : array->array) {
    if (value.kind == asset_json::Value::Kind::String) {
      values.push_back(value.string);
    } else if (value.kind == asset_json::Value::Kind::Object) {
      values.push_back(asset_json::textOr(value, "message"));
    }
  }
  return values;
}

[[nodiscard]] AssetProductionTextureAudit
textureAuditFromCooked(const CookedMaterialTextureRecord &texture) {
  AssetProductionTextureAudit audit;
  audit.role = texture.role;
  audit.source_path = texture.source_path;
  audit.cooked_path = texture.cooked_path;
  audit.kind = texture.kind;
  audit.color_space = texture.color_space;
  audit.source_format = texture.source_format;
  audit.runtime_format = texture.runtime_format;
  audit.width = texture.width;
  audit.height = texture.height;
  audit.mip_count = texture.mip_count;
  audit.byte_cost = texture.byte_cost;
  audit.encoder = texture.encoder;
  audit.fallback_reason = texture.fallback_reason;
  audit.platform_compatibility = texture.platform_compatibility;
  audit.source_hash = texture.source_hash;
  audit.cooked_hash = texture.cooked_hash;
  audit.present = !texture.cooked_path.empty() && pathExists(texture.cooked_path);
  audit.diagnostics = texture.diagnostics;
  if (!texture.cooked_path.empty() && !audit.present) {
    appendMissingFileDiagnostic(texture.cooked_path, audit.diagnostics);
  }
  if (!texture.fallback_reason.empty()) {
    audit.diagnostics.push_back("warning: fallback: " + texture.fallback_reason);
  }
  return audit;
}

[[nodiscard]] AssetProductionTextureAudit
textureAuditFromReport(const asset_json::Value &json, const std::filesystem::path &database_root) {
  AssetProductionTextureAudit audit;
  audit.role = asset_json::textOr(json, "role");
  audit.kind = asset_json::textOr(json, "kind", audit.role);
  audit.color_space = asset_json::textOr(json, "color_space");
  audit.source_format =
      asset_json::textOr(json, "source_format", asset_json::textOr(json, "format"));
  audit.runtime_format = asset_json::textOr(json, "runtime_format");
  audit.width = asset_json::u32Or(json, "width");
  audit.height = asset_json::u32Or(json, "height");
  audit.mip_count = asset_json::u32Or(json, "mip_count");
  audit.byte_cost = asset_json::u64Or(json, "byte_cost");
  audit.encoder = asset_json::textOr(json, "encoder");
  audit.fallback_reason = asset_json::textOr(json, "fallback_reason");
  audit.platform_compatibility = asset_json::textOr(json, "platform_compatibility");
  audit.source_hash = asset_json::textOr(json, "source_hash");
  audit.cooked_hash = asset_json::textOr(json, "cooked_hash");
  audit.source_path = asset_json::textOr(json, "source_path");
  const std::string cooked_path = asset_json::textOr(json, "cooked_path");
  if (!cooked_path.empty()) {
    audit.cooked_path = resolveRelative(database_root, cooked_path);
  }
  audit.present = !audit.cooked_path.empty() && pathExists(audit.cooked_path);
  audit.diagnostics = stringsFromArray(json, "diagnostics");
  if (!audit.cooked_path.empty() && !audit.present) {
    appendMissingFileDiagnostic(audit.cooked_path, audit.diagnostics);
  }
  if (!audit.fallback_reason.empty()) {
    audit.diagnostics.push_back("warning: fallback: " + audit.fallback_reason);
  }
  return audit;
}

[[nodiscard]] AssetProductionTextureAudit missingRequiredTexture(const std::string_view role) {
  AssetProductionTextureAudit audit;
  audit.role = std::string(role);
  audit.kind = std::string(role);
  audit.present = false;
  audit.diagnostics.push_back("error: missing required LitPBR texture role: " +
                              std::string(role));
  return audit;
}

[[nodiscard]] bool containsTextureRole(const std::vector<AssetProductionTextureAudit> &textures,
                                       const std::string_view role) {
  return std::any_of(textures.begin(), textures.end(), [&](const AssetProductionTextureAudit &t) {
    return t.role == role;
  });
}

void appendRequiredRoleRows(AssetProductionAsset &asset) {
  asset.material.required_roles.clear();
  for (const std::string_view role : kRequiredLitPbrRoles) {
    asset.material.required_roles.emplace_back(role);
    if (!containsTextureRole(asset.material.textures, role)) {
      AssetProductionTextureAudit missing = missingRequiredTexture(role);
      asset.material.diagnostics.push_back(missing.diagnostics.front());
      asset.material.textures.push_back(missing);
    }
  }
  asset.textures = asset.material.textures;
}

void loadMaterialAudit(AssetProductionAsset &asset, const AssetDatabaseRecord &record,
                       const std::filesystem::path &database_root) {
  asset.material.attempted = true;
  const std::optional<std::filesystem::path> material_bin =
      requiredOutputPath(record, database_root, "materialbin", "materialbin", "materialbin",
                         asset.model_diagnostics);
  const std::optional<std::filesystem::path> report =
      requiredOutputPath(record, database_root, "report", "json", "cook report",
                         asset.model_diagnostics);
  const std::optional<std::filesystem::path> preview =
      requiredOutputPath(record, database_root, "preview", "ppm", "preview",
                         asset.model_diagnostics);
  (void)report;

  if (preview.has_value()) {
    asset.preview = loadAssetPreviewImage(*preview);
    if (!asset.preview.available) {
      asset.model_diagnostics.push_back("error: " + asset.preview.diagnostic);
    }
  }

  if (!material_bin.has_value() || !pathExists(*material_bin)) {
    appendRequiredRoleRows(asset);
    return;
  }

  try {
    const CookedMaterialAsset cooked = loadCookedMaterialAsset(*material_bin);
    asset.material.loaded = true;
    asset.material.material_bin_path = cooked.material_bin_path;
    asset.material.asset_guid = cooked.asset_guid;
    asset.material.id = cooked.asset.id;
    asset.material.name = cooked.asset.name;
    asset.name = cooked.asset.name.empty() ? asset.name : cooked.asset.name;
    asset.material.shader_variant_tag = cooked.shader_variant_tag;
    asset.material.pipeline_tag = cooked.pipeline_tag;
    asset.material.feature_mask = cooked.feature_mask;
    asset.material.shader_variant_key = cooked.shader_variant_key;
    for (const CookedMaterialTextureRecord &texture : cooked.textures) {
      asset.material.textures.push_back(textureAuditFromCooked(texture));
    }
    for (const AssetCookDiagnostic &diagnostic : cooked.diagnostics) {
      asset.material.diagnostics.push_back(diagnostic.severity + ": " + diagnostic.message);
    }
  } catch (const std::exception &error) {
    asset.material.diagnostics.push_back("error: could not load materialbin: " +
                                         std::string(error.what()));
  }
  appendRequiredRoleRows(asset);
}

void loadTextureAudit(AssetProductionAsset &asset, const AssetDatabaseRecord &record,
                      const std::filesystem::path &database_root) {
  const std::optional<std::filesystem::path> texture =
      requiredOutputPath(record, database_root, "texture", "ktx2", "runtime texture",
                         asset.model_diagnostics);
  const std::optional<std::filesystem::path> report =
      requiredOutputPath(record, database_root, "report", "json", "texture report",
                         asset.model_diagnostics);
  if (!report.has_value() || !pathExists(*report)) {
    return;
  }
  try {
    AssetProductionTextureAudit audit =
        textureAuditFromReport(asset_json::parseFile(*report), database_root);
    if (texture.has_value()) {
      audit.cooked_path = *texture;
      audit.present = pathExists(*texture);
    }
    asset.textures.push_back(std::move(audit));
  } catch (const std::exception &error) {
    asset.model_diagnostics.push_back("error: could not load texture report: " +
                                      std::string(error.what()));
  }
}

void loadMeshAudit(AssetProductionAsset &asset, const AssetDatabaseRecord &record,
                   const std::filesystem::path &database_root) {
  asset.mesh.attempted = true;
  const std::optional<std::filesystem::path> cache =
      requiredOutputPath(record, database_root, "runtime-cache", "astercache",
                         "scene runtime cache", asset.model_diagnostics);
  const std::optional<std::filesystem::path> report =
      requiredOutputPath(record, database_root, "report", "json", "scene cook report",
                         asset.model_diagnostics);
  (void)report;
  if (!cache.has_value() || !pathExists(*cache)) {
    return;
  }
  try {
    const SceneAsset scene_asset = loadCompiledSceneAsset(*cache);
    const SceneAssetCacheMetadata &metadata = scene_asset.cache_metadata;
    asset.mesh.loaded = metadata.valid;
    asset.mesh.cache_path = *cache;
    asset.mesh.material_count = metadata.material_count;
    asset.mesh.mesh_count = metadata.mesh_count;
    asset.mesh.collision_mesh_count = metadata.collision_mesh_count;
    asset.mesh.scene_node_count = metadata.scene_node_count;
    asset.mesh.total_vertices = metadata.total_vertices;
    asset.mesh.total_indices = metadata.total_indices;
    asset.mesh.total_collision_triangles = metadata.total_collision_triangles;
    asset.mesh.diagnostics = metadata.diagnostics;
    if (!metadata.valid) {
      asset.mesh.messages.push_back("error: scene cache metadata is invalid");
    }
  } catch (const std::exception &error) {
    asset.mesh.messages.push_back("error: could not load scene cache: " +
                                  std::string(error.what()));
  }
}

[[nodiscard]] AssetProductionCookAudit cookAuditFromRecord(const AssetDatabaseRecord &record) {
  AssetProductionCookAudit audit;
  audit.diagnostics = record.diagnostics;
  audit.dependency_edges = record.dependency_edges;
  audit.outputs = record.outputs;
  audit.hashes = record.derived_hashes;
  audit.import_preset = record.import_preset;
  audit.platform_profile = record.platform_profile;
  for (const AssetCookDiagnostic &diagnostic : record.diagnostics) {
    if (diagnostic.severity == "error") {
      ++audit.error_count;
    } else if (diagnostic.severity == "warning") {
      ++audit.warning_count;
    }
  }
  return audit;
}

void verifyDeclaredOutputs(const AssetDatabaseRecord &record, const std::filesystem::path &root,
                           std::vector<std::string> &diagnostics) {
  for (const AssetCookedOutput &output : record.outputs) {
    const std::filesystem::path path = resolveRelative(root, output.path);
    if (!pathExists(path)) {
      appendMissingFileDiagnostic(path, diagnostics);
    }
  }
}

[[nodiscard]] bool hasErrorDiagnostics(const std::vector<std::string> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(), [](const std::string &diagnostic) {
    return diagnostic.rfind("error:", 0u) == 0u;
  });
}

[[nodiscard]] AssetProductionAsset productionAssetFromRecord(
    const AssetDatabaseRecord &record, const std::filesystem::path &database_root) {
  AssetProductionAsset asset;
  asset.guid = record.guid;
  asset.id = record.id;
  asset.name = record.id;
  asset.kind = record.kind;
  asset.source_path = record.source_path;
  asset.cook = cookAuditFromRecord(record);
  verifyDeclaredOutputs(record, database_root, asset.model_diagnostics);

  if (record.kind == "material") {
    loadMaterialAudit(asset, record, database_root);
  } else if (record.kind == "texture") {
    loadTextureAudit(asset, record, database_root);
  } else if (record.kind == "scene") {
    loadMeshAudit(asset, record, database_root);
  } else {
    if (record.outputs.empty()) {
      asset.model_diagnostics.push_back("warning: no cooked outputs for asset kind: " +
                                        record.kind);
    }
  }

  asset.production_ready = record.fate_report.production_ready && !asset.hasErrors();
  return asset;
}

[[nodiscard]] bool parseUint(const std::string_view text, std::uint32_t &out) {
  const char *begin = text.data();
  const char *end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

class PpmTokenReader {
public:
  explicit PpmTokenReader(std::string_view bytes) : bytes_(bytes) {}

  [[nodiscard]] std::optional<std::string_view> next() {
    skipTrivia();
    if (position_ >= bytes_.size()) {
      return std::nullopt;
    }
    const std::size_t start = position_;
    while (position_ < bytes_.size() &&
           !std::isspace(static_cast<unsigned char>(bytes_[position_]))) {
      if (bytes_[position_] == '#') {
        break;
      }
      ++position_;
    }
    return bytes_.substr(start, position_ - start);
  }

  void skipTrivia() {
    while (position_ < bytes_.size()) {
      const unsigned char c = static_cast<unsigned char>(bytes_[position_]);
      if (std::isspace(c)) {
        ++position_;
        continue;
      }
      if (bytes_[position_] == '#') {
        while (position_ < bytes_.size() && bytes_[position_] != '\n') {
          ++position_;
        }
        continue;
      }
      break;
    }
  }

  [[nodiscard]] std::size_t position() const noexcept {
    return position_;
  }

private:
  std::string_view bytes_;
  std::size_t position_ = 0u;
};

} // namespace

const AssetProductionTextureAudit *
AssetProductionAsset::findTexture(const std::string_view role) const noexcept {
  const auto found = std::find_if(textures.begin(), textures.end(),
                                  [&](const AssetProductionTextureAudit &texture) {
                                    return texture.role == role;
                                  });
  return found == textures.end() ? nullptr : &*found;
}

bool AssetProductionAsset::hasErrors() const noexcept {
  if (cook.error_count > 0u || hasErrorDiagnostics(model_diagnostics) ||
      hasErrorDiagnostics(material.diagnostics) || hasErrorDiagnostics(mesh.messages)) {
    return true;
  }
  return std::any_of(textures.begin(), textures.end(), [](const AssetProductionTextureAudit &t) {
    return hasSeverity(t, "error");
  });
}

AssetProductionModel AssetProductionModel::fromDatabase(
    const AssetDatabase &database, const std::filesystem::path &database_root) {
  AssetProductionModel model;
  model.root_path = database_root;
  model.assets.reserve(database.records.size());
  for (const AssetDatabaseRecord &record : database.records) {
    model.assets.push_back(productionAssetFromRecord(record, database_root));
  }
  return model;
}

const AssetProductionAsset *AssetProductionModel::find(
    const std::string_view id_or_guid) const noexcept {
  const auto found = std::find_if(assets.begin(), assets.end(),
                                  [&](const AssetProductionAsset &asset) {
                                    return asset.id == id_or_guid || asset.guid == id_or_guid;
                                  });
  return found == assets.end() ? nullptr : &*found;
}

AssetPreviewImage loadAssetPreviewImage(const std::filesystem::path &path) {
  AssetPreviewImage image;
  image.path = path;
  try {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      image.diagnostic = "could not open preview: " + path.generic_string();
      return image;
    }
    const std::string bytes{std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>()};
    PpmTokenReader reader(bytes);
    const std::optional<std::string_view> magic = reader.next();
    if (!magic.has_value() || (*magic != "P3" && *magic != "P6")) {
      image.diagnostic = "preview is not a P3/P6 PPM: " + path.generic_string();
      return image;
    }
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t max_value = 0u;
    const std::optional<std::string_view> width_token = reader.next();
    const std::optional<std::string_view> height_token = reader.next();
    const std::optional<std::string_view> max_token = reader.next();
    if (!width_token.has_value() || !height_token.has_value() || !max_token.has_value() ||
        !parseUint(*width_token, width) || !parseUint(*height_token, height) ||
        !parseUint(*max_token, max_value) || width == 0u || height == 0u || max_value == 0u ||
        max_value > 255u) {
      image.diagnostic = "preview PPM header is invalid: " + path.generic_string();
      return image;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    image.width = width;
    image.height = height;
    image.rgba8.assign(pixel_count * 4u, 255u);
    const auto scale = [&](const std::uint32_t value) {
      return static_cast<std::uint8_t>(
          std::min<std::uint32_t>(255u, (value * 255u + max_value / 2u) / max_value));
    };

    if (*magic == "P3") {
      for (std::size_t pixel = 0u; pixel < pixel_count; ++pixel) {
        for (std::size_t channel = 0u; channel < 3u; ++channel) {
          const std::optional<std::string_view> token = reader.next();
          std::uint32_t value = 0u;
          if (!token.has_value() || !parseUint(*token, value) || value > max_value) {
            image.available = false;
            image.rgba8.clear();
            image.diagnostic = "preview PPM pixel data is invalid: " + path.generic_string();
            return image;
          }
          image.rgba8[pixel * 4u + channel] = scale(value);
        }
      }
    } else {
      reader.skipTrivia();
      const std::size_t payload = pixel_count * 3u;
      if (reader.position() + payload > bytes.size()) {
        image.available = false;
        image.rgba8.clear();
        image.diagnostic = "preview PPM pixel payload is truncated: " + path.generic_string();
        return image;
      }
      for (std::size_t pixel = 0u; pixel < pixel_count; ++pixel) {
        for (std::size_t channel = 0u; channel < 3u; ++channel) {
          image.rgba8[pixel * 4u + channel] =
              scale(static_cast<unsigned char>(bytes[reader.position() + pixel * 3u + channel]));
        }
      }
    }

    image.available = true;
  } catch (const std::exception &error) {
    image.available = false;
    image.rgba8.clear();
    image.diagnostic = "could not load preview: " + std::string(error.what());
  }
  return image;
}

} // namespace aster
