// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_database.hpp"

#include "aster/asset/json_document.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace aster {
namespace {

using asset_json::Value;

[[nodiscard]] std::filesystem::path resolveRelative(const std::filesystem::path &base,
                                                    const std::string &path) {
  std::filesystem::path value(path);
  return value.is_absolute() ? value : base / value;
}

[[nodiscard]] const Value *arrayField(const Value &json, const std::string_view key) {
  const Value *value = json.find(key);
  return value != nullptr && value->kind == Value::Kind::Array ? value : nullptr;
}

[[nodiscard]] AssetSourceRecord sourceFrom(const Value &json) {
  if (const Value *source = json.find("source");
      source != nullptr && source->kind == Value::Kind::Object) {
    return {.path = asset_json::textOr(*source, "path"),
            .hash = asset_json::textOr(*source, "hash")};
  }
  return {.path = asset_json::textOr(json, "source_path"),
          .hash = asset_json::textOr(json, "source_hash")};
}

[[nodiscard]] AssetImportPresetRecord importPresetFrom(const Value &json) {
  const Value *preset = json.find("import_preset");
  if (preset == nullptr || preset->kind != Value::Kind::Object) {
    return {};
  }
  return {.name = asset_json::textOr(*preset, "name"),
          .origin_policy = asset_json::textOr(*preset, "origin_policy"),
          .unit_scale = asset_json::textOr(*preset, "unit_scale"),
          .texture_role_policy = asset_json::textOr(*preset, "texture_role_policy"),
          .material_slot_policy = asset_json::textOr(*preset, "material_slot_policy"),
          .collision_policy = asset_json::textOr(*preset, "collision_policy"),
          .lod_policy = asset_json::textOr(*preset, "lod_policy"),
          .meshlet_policy = asset_json::textOr(*preset, "meshlet_policy"),
          .skeleton_policy = asset_json::textOr(*preset, "skeleton_policy"),
          .animation_policy = asset_json::textOr(*preset, "animation_policy"),
          .morph_policy = asset_json::textOr(*preset, "morph_policy")};
}

[[nodiscard]] AssetPlatformProfileRecord platformProfileFrom(const Value &json) {
  const Value *profile = json.find("platform_profile");
  if (profile == nullptr || profile->kind != Value::Kind::Object) {
    return {};
  }
  return {.name = asset_json::textOr(*profile, "name"),
          .runtime_texture_format = asset_json::textOr(*profile, "runtime_texture_format"),
          .compression = asset_json::textOr(*profile, "compression"),
          .target = asset_json::textOr(*profile, "target")};
}

[[nodiscard]] std::vector<AssetToolVersionRecord> toolVersionsFrom(const Value &json) {
  std::vector<AssetToolVersionRecord> versions;
  if (const Value *values = arrayField(json, "tool_versions")) {
    for (const Value &value : values->array) {
      versions.push_back({.name = asset_json::textOr(value, "name"),
                          .version = asset_json::textOr(value, "version")});
    }
  }
  return versions;
}

[[nodiscard]] std::vector<AssetSourceLocation> sourceLocationsFrom(const Value &json) {
  std::vector<AssetSourceLocation> locations;
  if (const Value *values = arrayField(json, "source_locations")) {
    for (const Value &value : values->array) {
      locations.push_back({.source_path = asset_json::textOr(value, "source_path"),
                           .line = asset_json::u32Or(value, "line"),
                           .column = asset_json::u32Or(value, "column")});
    }
  }
  return locations;
}

[[nodiscard]] std::vector<AssetCookDiagnostic> diagnosticsFrom(const Value &json) {
  std::vector<AssetCookDiagnostic> diagnostics;
  if (const Value *values = arrayField(json, "diagnostics")) {
    for (const Value &value : values->array) {
      diagnostics.push_back({.severity = asset_json::textOr(value, "severity"),
                             .message = asset_json::textOr(value, "message"),
                             .source_path = asset_json::textOr(value, "source_path"),
                             .line = asset_json::u32Or(value, "line"),
                             .column = asset_json::u32Or(value, "column"),
                             .source_locations = sourceLocationsFrom(value)});
    }
  }
  return diagnostics;
}

[[nodiscard]] std::vector<AssetDependencyRecord> dependenciesFrom(const Value &json) {
  std::vector<AssetDependencyRecord> dependencies;
  const Value *values = arrayField(json, "dependencies");
  if (values == nullptr) {
    values = arrayField(json, "dependency_hashes");
  }
  if (values != nullptr) {
    for (const Value &value : values->array) {
      dependencies.push_back({.role = asset_json::textOr(value, "role"),
                              .path = asset_json::textOr(value, "path"),
                              .present = asset_json::boolOr(value, "present"),
                              .hash = asset_json::textOr(value, "hash")});
    }
  }
  return dependencies;
}

[[nodiscard]] std::vector<AssetDependencyEdge> dependencyEdgesFrom(const Value &json) {
  std::vector<AssetDependencyEdge> edges;
  if (const Value *values = arrayField(json, "dependency_edges")) {
    for (const Value &value : values->array) {
      edges.push_back({.from = asset_json::textOr(value, "from"),
                       .to = asset_json::textOr(value, "to"),
                       .role = asset_json::textOr(value, "role"),
                       .present = asset_json::boolOr(value, "present"),
                       .hash = asset_json::textOr(value, "hash")});
    }
  }
  return edges;
}

[[nodiscard]] std::vector<AssetCookedOutput> outputsFrom(const Value &json) {
  std::vector<AssetCookedOutput> outputs;
  if (const Value *values = arrayField(json, "outputs")) {
    for (const Value &value : values->array) {
      outputs.push_back({.role = asset_json::textOr(value, "role"),
                         .kind = asset_json::textOr(value, "kind"),
                         .path = asset_json::textOr(value, "path"),
                         .hash = asset_json::textOr(value, "hash")});
    }
  }
  return outputs;
}

[[nodiscard]] std::vector<AssetArtifactRecord> artifactsFrom(const Value &json) {
  std::vector<AssetArtifactRecord> artifacts;
  if (const Value *values = arrayField(json, "artifacts")) {
    for (const Value &value : values->array) {
      artifacts.push_back({.role = asset_json::textOr(value, "role"),
                           .kind = asset_json::textOr(value, "kind"),
                           .path = asset_json::textOr(value, "path"),
                           .hash = asset_json::textOr(value, "hash"),
                           .reused = asset_json::boolOr(value, "reused")});
    }
  }
  return artifacts;
}

void backfillV2Record(AssetDatabaseRecord &record) {
  if (record.source.path.empty()) {
    record.source.path = record.source_path;
  }
  if (record.source.hash.empty()) {
    record.source.hash = record.source_hash;
  }
  if (record.artifacts.empty()) {
    for (const AssetCookedOutput &output : record.outputs) {
      record.artifacts.push_back({.role = output.role,
                                  .kind = output.kind,
                                  .path = output.path,
                                  .hash = output.hash});
    }
  }
  if (record.dependency_edges.empty()) {
    for (const AssetDependencyRecord &dependency : record.dependencies) {
      record.dependency_edges.push_back({.from = record.guid,
                                         .to = dependency.path,
                                         .role = dependency.role,
                                         .present = dependency.present,
                                         .hash = dependency.hash});
    }
  }
}

[[nodiscard]] MaterialSurfaceProfile parseSurfaceProfile(const std::string &value) {
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::replace(normalized.begin(), normalized.end(), '_', '-');
  if (normalized == "stratifiedrock" || normalized == "stratified-rock" ||
      normalized == "rock") {
    return MaterialSurfaceProfile::StratifiedRock;
  }
  if (normalized == "corrodedmetal" || normalized == "corroded-metal" ||
      normalized == "weathered-metal") {
    return MaterialSurfaceProfile::CorrodedMetal;
  }
  if (normalized == "emissivelens" || normalized == "emissive-lens") {
    return MaterialSurfaceProfile::EmissiveLens;
  }
  if (normalized == "plain" || normalized == "none") {
    return MaterialSurfaceProfile::Plain;
  }
  return MaterialSurfaceProfile::Auto;
}

[[nodiscard]] MaterialBlendMode parseBlendMode(const std::string &value) {
  if (value == "Masked" || value == "masked" || value == "AlphaClip") {
    return MaterialBlendMode::Masked;
  }
  if (value == "Blend" || value == "blend") {
    return MaterialBlendMode::Blend;
  }
  return MaterialBlendMode::Opaque;
}

} // namespace

AssetDatabase loadAssetDatabase(const std::filesystem::path &path) {
  const Value root = asset_json::parseFile(path);
  AssetDatabase database;
  database.source_path = path;
  database.schema_version = asset_json::u32Or(root, "schema_version");
  database.platform = asset_json::textOr(root, "platform");
  database.artifact_manifest = asset_json::textOr(root, "artifact_manifest");
  database.tool_versions = toolVersionsFrom(root);
  const Value &records = asset_json::required(root, "records");
  if (records.kind != Value::Kind::Array) {
    throw asset_json::Error("asset database records must be an array", records.location);
  }
  for (const Value &record_json : records.array) {
    AssetDatabaseRecord record;
    record.guid = asset_json::textOr(record_json, "guid");
    record.id = asset_json::textOr(record_json, "id");
    record.kind = asset_json::textOr(record_json, "kind");
    record.source = sourceFrom(record_json);
    record.source_path = asset_json::textOr(record_json, "source_path", record.source.path);
    record.import_preset = importPresetFrom(record_json);
    record.platform_profile = platformProfileFrom(record_json);
    record.import_settings_version = asset_json::u32Or(record_json, "import_settings_version");
    record.source_hash = asset_json::textOr(record_json, "source_hash", record.source.hash);
    record.options_hash = asset_json::textOr(record_json, "options_hash");
    record.dependency_edges = dependencyEdgesFrom(record_json);
    record.dependencies = dependenciesFrom(record_json);
    record.artifacts = artifactsFrom(record_json);
    record.outputs = outputsFrom(record_json);
    record.diagnostics = diagnosticsFrom(record_json);
    record.tool_versions = toolVersionsFrom(record_json);
    record.platform = asset_json::textOr(record_json, "platform", database.platform);
    backfillV2Record(record);
    database.records.push_back(std::move(record));
  }
  return database;
}

const AssetDatabaseRecord *findAssetRecord(const AssetDatabase &database,
                                           const std::string_view id) {
  const auto found = std::find_if(database.records.begin(), database.records.end(),
                                  [&](const AssetDatabaseRecord &record) {
                                    return record.id == id || record.guid == id;
                                  });
  return found == database.records.end() ? nullptr : &*found;
}

std::optional<std::filesystem::path>
findAssetOutputPath(const AssetDatabaseRecord &record, const std::filesystem::path &database_root,
                    const std::string_view role, const std::string_view kind) {
  const auto found = std::find_if(record.outputs.begin(), record.outputs.end(),
                                  [&](const AssetCookedOutput &output) {
                                    return output.role == role &&
                                           (kind.empty() || output.kind == kind);
                                  });
  if (found == record.outputs.end()) {
    return std::nullopt;
  }
  return resolveRelative(database_root, found->path);
}

CookedMaterialAsset loadCookedMaterialAsset(const std::filesystem::path &path) {
  const Value root = asset_json::parseFile(path);
  const std::filesystem::path root_path = path.parent_path();
  CookedMaterialAsset cooked;
  cooked.material_bin_path = path;
  cooked.asset_guid = asset_json::textOr(root, "asset_guid");
  cooked.shader_variant_tag = asset_json::textOr(root, "shader_variant_tag");
  cooked.pipeline_tag = asset_json::textOr(root, "pipeline_tag");
  cooked.feature_mask = asset_json::u64Or(root, "feature_mask");
  cooked.shader_variant_key = asset_json::u64Or(root, "shader_variant_key");
  cooked.dependencies = dependenciesFrom(root);
  cooked.diagnostics = diagnosticsFrom(root);

  cooked.asset.id = asset_json::textOr(root, "id");
  cooked.asset.name = asset_json::textOr(root, "name", cooked.asset.id);
  cooked.asset.source_path = path;
  if (const Value *fallback = root.find("fallback"); fallback != nullptr) {
    cooked.asset.surface_profile =
        parseSurfaceProfile(asset_json::textOr(*fallback, "surface_profile"));
    cooked.asset.blend_mode = parseBlendMode(asset_json::textOr(*fallback, "alpha_mode"));
    cooked.asset.cull_mode = asset_json::boolOr(*fallback, "double_sided")
                                 ? MaterialAssetCullMode::None
                                 : MaterialAssetCullMode::Back;
    cooked.asset.receives_shadows = asset_json::boolOr(*fallback, "receives_shadows", true);
    cooked.asset.params["base_color_r"] = asset_json::f32Or(*fallback, "base_color_r", 1.0f);
    if (const Value *base = fallback->find("base_color");
        base != nullptr && base->kind == Value::Kind::Array && base->array.size() >= 3u) {
      cooked.asset.params["base_color_r"] = static_cast<float>(base->array[0].number);
      cooked.asset.params["base_color_g"] = static_cast<float>(base->array[1].number);
      cooked.asset.params["base_color_b"] = static_cast<float>(base->array[2].number);
    }
    cooked.asset.params["roughness"] = asset_json::f32Or(*fallback, "roughness", 0.55f);
    cooked.asset.params["metallic"] = asset_json::f32Or(*fallback, "metallic", 0.0f);
    cooked.asset.params["opacity"] = asset_json::f32Or(*fallback, "opacity", 1.0f);
    cooked.asset.params["emission_strength"] =
        asset_json::f32Or(*fallback, "emission_strength", 0.0f);
  }
  if (const Value *params = root.find("params");
      params != nullptr && params->kind == Value::Kind::Object) {
    for (const auto &[key, value] : params->object) {
      if (value.kind == Value::Kind::Number) {
        cooked.asset.params[key] = static_cast<float>(value.number);
      }
    }
  }
  if (const Value *features = root.find("features");
      features != nullptr && features->kind == Value::Kind::Object) {
    for (const auto &[key, value] : features->object) {
      if (value.kind == Value::Kind::Bool) {
        cooked.asset.explicit_features[key] = value.boolean;
      }
    }
  }
  if (const Value *textures = root.find("textures");
      textures != nullptr && textures->kind == Value::Kind::Array) {
    for (const Value &texture_json : textures->array) {
      CookedMaterialTextureRecord texture;
      texture.role = asset_json::textOr(texture_json, "role");
      texture.source_path = asset_json::textOr(texture_json, "source_path");
      texture.cooked_path = std::filesystem::absolute(resolveRelative(
          root_path.parent_path(), asset_json::textOr(texture_json, "cooked_path")));
      texture.kind = asset_json::textOr(texture_json, "kind");
      texture.color_space = asset_json::textOr(texture_json, "color_space");
      texture.source_format =
          asset_json::textOr(texture_json, "source_format", asset_json::textOr(texture_json, "format"));
      texture.runtime_format = asset_json::textOr(texture_json, "runtime_format", "ktx2");
      texture.width = asset_json::u32Or(texture_json, "width");
      texture.height = asset_json::u32Or(texture_json, "height");
      texture.mip_count = asset_json::u32Or(texture_json, "mip_count");
      texture.byte_cost = asset_json::u64Or(texture_json, "byte_cost");
      texture.encoder = asset_json::textOr(texture_json, "encoder");
      texture.fallback_reason = asset_json::textOr(texture_json, "fallback_reason");
      texture.platform_compatibility =
          asset_json::textOr(texture_json, "platform_compatibility");
      texture.source_hash = asset_json::textOr(texture_json, "source_hash");
      texture.cooked_hash = asset_json::textOr(texture_json, "cooked_hash");
      if (const Value *diagnostics = arrayField(texture_json, "diagnostics")) {
        for (const Value &diagnostic : diagnostics->array) {
          if (diagnostic.kind == Value::Kind::String) {
            texture.diagnostics.push_back(diagnostic.string);
          }
        }
      }
      MaterialTextureSlot slot;
      slot.role = texture.role;
      slot.uri = texture.cooked_path;
      slot.srgb = texture.color_space == "srgb";
      slot.required = true;
      cooked.asset.textures[texture.role] = std::move(slot);
      cooked.textures.push_back(std::move(texture));
    }
  }
  return cooked;
}

} // namespace aster
