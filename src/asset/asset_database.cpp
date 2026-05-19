// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/asset_database.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string_view>

namespace aster {
namespace {

struct Json {
  enum class Kind {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
  };

  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Json> array;
  std::map<std::string, Json> object;

  [[nodiscard]] const Json *find(const std::string_view key) const {
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
  }
};

class JsonParser {
public:
  explicit JsonParser(std::string_view source) : source_(source) {}

  [[nodiscard]] Json parse() {
    Json value = parseValue();
    skipWhitespace();
    if (position_ != source_.size()) {
      throw std::runtime_error("asset JSON has trailing data");
    }
    return value;
  }

private:
  void skipWhitespace() {
    while (position_ < source_.size() &&
           std::isspace(static_cast<unsigned char>(source_[position_]))) {
      ++position_;
    }
  }

  [[nodiscard]] char peek() {
    skipWhitespace();
    if (position_ >= source_.size()) {
      throw std::runtime_error("asset JSON ended unexpectedly");
    }
    return source_[position_];
  }

  bool consume(const char expected) {
    skipWhitespace();
    if (position_ < source_.size() && source_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  void require(const char expected) {
    if (!consume(expected)) {
      throw std::runtime_error("asset JSON has unexpected syntax");
    }
  }

  [[nodiscard]] Json parseValue() {
    const char c = peek();
    if (c == '{') {
      return parseObject();
    }
    if (c == '[') {
      return parseArray();
    }
    if (c == '"') {
      Json value;
      value.kind = Json::Kind::String;
      value.string = parseString();
      return value;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parseNumber();
    }
    if (source_.substr(position_, 4) == "true") {
      position_ += 4u;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = true;
      return value;
    }
    if (source_.substr(position_, 5) == "false") {
      position_ += 5u;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = false;
      return value;
    }
    if (source_.substr(position_, 4) == "null") {
      position_ += 4u;
      return {};
    }
    throw std::runtime_error("asset JSON contains an unsupported value");
  }

  [[nodiscard]] Json parseObject() {
    Json value;
    value.kind = Json::Kind::Object;
    require('{');
    if (consume('}')) {
      return value;
    }
    for (;;) {
      std::string key = parseString();
      require(':');
      value.object.emplace(std::move(key), parseValue());
      if (consume('}')) {
        return value;
      }
      require(',');
    }
  }

  [[nodiscard]] Json parseArray() {
    Json value;
    value.kind = Json::Kind::Array;
    require('[');
    if (consume(']')) {
      return value;
    }
    for (;;) {
      value.array.push_back(parseValue());
      if (consume(']')) {
        return value;
      }
      require(',');
    }
  }

  [[nodiscard]] Json parseNumber() {
    Json value;
    value.kind = Json::Kind::Number;
    const std::size_t start = position_;
    if (source_[position_] == '-') {
      ++position_;
    }
    while (position_ < source_.size() &&
           std::isdigit(static_cast<unsigned char>(source_[position_]))) {
      ++position_;
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      ++position_;
      while (position_ < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[position_]))) {
        ++position_;
      }
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      ++position_;
      if (position_ < source_.size() && (source_[position_] == '-' || source_[position_] == '+')) {
        ++position_;
      }
      while (position_ < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[position_]))) {
        ++position_;
      }
    }
    value.number = std::stod(std::string(source_.substr(start, position_ - start)));
    return value;
  }

  [[nodiscard]] std::string parseString() {
    require('"');
    std::string out;
    while (position_ < source_.size()) {
      const char c = source_[position_++];
      if (c == '"') {
        return out;
      }
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (position_ >= source_.size()) {
        throw std::runtime_error("asset JSON has an unterminated escape");
      }
      const char escaped = source_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        out.push_back(escaped);
        break;
      case 'b':
        out.push_back('\b');
        break;
      case 'f':
        out.push_back('\f');
        break;
      case 'n':
        out.push_back('\n');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case 't':
        out.push_back('\t');
        break;
      default:
        out.push_back(escaped);
        break;
      }
    }
    throw std::runtime_error("asset JSON has an unterminated string");
  }

  std::string_view source_;
  std::size_t position_ = 0u;
};

[[nodiscard]] std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not open asset file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] const Json &required(const Json &json, const std::string_view key) {
  const Json *value = json.find(key);
  if (value == nullptr) {
    throw std::runtime_error("asset JSON is missing required key: " + std::string(key));
  }
  return *value;
}

[[nodiscard]] std::string textOr(const Json &json, const std::string_view key,
                                 const std::string_view fallback = {}) {
  const Json *value = json.find(key);
  return value != nullptr && value->kind == Json::Kind::String ? value->string
                                                               : std::string(fallback);
}

[[nodiscard]] std::uint32_t u32Or(const Json &json, const std::string_view key,
                                  const std::uint32_t fallback = 0u) {
  const Json *value = json.find(key);
  return value != nullptr && value->kind == Json::Kind::Number
             ? static_cast<std::uint32_t>(std::max(value->number, 0.0))
             : fallback;
}

[[nodiscard]] std::uint64_t u64Or(const Json &json, const std::string_view key,
                                  const std::uint64_t fallback = 0u) {
  const Json *value = json.find(key);
  return value != nullptr && value->kind == Json::Kind::Number
             ? static_cast<std::uint64_t>(std::max(value->number, 0.0))
             : fallback;
}

[[nodiscard]] float f32Or(const Json &json, const std::string_view key, const float fallback = 0.0f) {
  const Json *value = json.find(key);
  return value != nullptr && value->kind == Json::Kind::Number ? static_cast<float>(value->number)
                                                               : fallback;
}

[[nodiscard]] bool boolOr(const Json &json, const std::string_view key, const bool fallback = false) {
  const Json *value = json.find(key);
  return value != nullptr && value->kind == Json::Kind::Bool ? value->boolean : fallback;
}

[[nodiscard]] std::vector<AssetCookDiagnostic> diagnosticsFrom(const Json &json) {
  std::vector<AssetCookDiagnostic> diagnostics;
  if (const Json *values = json.find("diagnostics");
      values != nullptr && values->kind == Json::Kind::Array) {
    for (const Json &value : values->array) {
      diagnostics.push_back({.severity = textOr(value, "severity"),
                             .message = textOr(value, "message")});
    }
  }
  return diagnostics;
}

[[nodiscard]] std::vector<AssetDependencyRecord> dependenciesFrom(const Json &json) {
  std::vector<AssetDependencyRecord> dependencies;
  const Json *values = json.find("dependencies");
  if (values == nullptr) {
    values = json.find("dependency_hashes");
  }
  if (values != nullptr && values->kind == Json::Kind::Array) {
    for (const Json &value : values->array) {
      dependencies.push_back({.role = textOr(value, "role"),
                              .path = textOr(value, "path"),
                              .present = boolOr(value, "present"),
                              .hash = textOr(value, "hash")});
    }
  }
  return dependencies;
}

[[nodiscard]] std::vector<AssetCookedOutput> outputsFrom(const Json &json) {
  std::vector<AssetCookedOutput> outputs;
  if (const Json *values = json.find("outputs");
      values != nullptr && values->kind == Json::Kind::Array) {
    for (const Json &value : values->array) {
      outputs.push_back({.role = textOr(value, "role"),
                         .kind = textOr(value, "kind"),
                         .path = textOr(value, "path"),
                         .hash = textOr(value, "hash")});
    }
  }
  return outputs;
}

[[nodiscard]] std::filesystem::path resolveRelative(const std::filesystem::path &base,
                                                    const std::string &path) {
  std::filesystem::path value(path);
  return value.is_absolute() ? value : base / value;
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
  const Json root = JsonParser(readTextFile(path)).parse();
  AssetDatabase database;
  database.source_path = path;
  database.schema_version = u32Or(root, "schema_version");
  database.platform = textOr(root, "platform");
  const Json &records = required(root, "records");
  if (records.kind != Json::Kind::Array) {
    throw std::runtime_error("asset database records must be an array");
  }
  for (const Json &record_json : records.array) {
    AssetDatabaseRecord record;
    record.guid = textOr(record_json, "guid");
    record.id = textOr(record_json, "id");
    record.kind = textOr(record_json, "kind");
    record.source_path = textOr(record_json, "source_path");
    record.import_settings_version = u32Or(record_json, "import_settings_version");
    record.source_hash = textOr(record_json, "source_hash");
    record.options_hash = textOr(record_json, "options_hash");
    record.dependencies = dependenciesFrom(record_json);
    record.outputs = outputsFrom(record_json);
    record.diagnostics = diagnosticsFrom(record_json);
    record.platform = textOr(record_json, "platform", database.platform);
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
  const Json root = JsonParser(readTextFile(path)).parse();
  const std::filesystem::path root_path = path.parent_path();
  CookedMaterialAsset cooked;
  cooked.material_bin_path = path;
  cooked.asset_guid = textOr(root, "asset_guid");
  cooked.shader_variant_tag = textOr(root, "shader_variant_tag");
  cooked.pipeline_tag = textOr(root, "pipeline_tag");
  cooked.feature_mask = u64Or(root, "feature_mask");
  cooked.shader_variant_key = u64Or(root, "shader_variant_key");
  cooked.dependencies = dependenciesFrom(root);
  cooked.diagnostics = diagnosticsFrom(root);

  cooked.asset.id = textOr(root, "id");
  cooked.asset.name = textOr(root, "name", cooked.asset.id);
  cooked.asset.source_path = path;
  if (const Json *fallback = root.find("fallback"); fallback != nullptr) {
    cooked.asset.surface_profile = parseSurfaceProfile(textOr(*fallback, "surface_profile"));
    cooked.asset.blend_mode = parseBlendMode(textOr(*fallback, "alpha_mode"));
    cooked.asset.cull_mode =
        boolOr(*fallback, "double_sided") ? MaterialAssetCullMode::None : MaterialAssetCullMode::Back;
    cooked.asset.receives_shadows = boolOr(*fallback, "receives_shadows", true);
    cooked.asset.params["base_color_r"] = f32Or(*fallback, "base_color_r", 1.0f);
    if (const Json *base = fallback->find("base_color");
        base != nullptr && base->kind == Json::Kind::Array && base->array.size() >= 3u) {
      cooked.asset.params["base_color_r"] = static_cast<float>(base->array[0].number);
      cooked.asset.params["base_color_g"] = static_cast<float>(base->array[1].number);
      cooked.asset.params["base_color_b"] = static_cast<float>(base->array[2].number);
    }
    cooked.asset.params["roughness"] = f32Or(*fallback, "roughness", 0.55f);
    cooked.asset.params["metallic"] = f32Or(*fallback, "metallic", 0.0f);
    cooked.asset.params["opacity"] = f32Or(*fallback, "opacity", 1.0f);
    cooked.asset.params["emission_strength"] = f32Or(*fallback, "emission_strength", 0.0f);
  }
  if (const Json *params = root.find("params"); params != nullptr && params->kind == Json::Kind::Object) {
    for (const auto &[key, value] : params->object) {
      if (value.kind == Json::Kind::Number) {
        cooked.asset.params[key] = static_cast<float>(value.number);
      }
    }
  }
  if (const Json *features = root.find("features");
      features != nullptr && features->kind == Json::Kind::Object) {
    for (const auto &[key, value] : features->object) {
      if (value.kind == Json::Kind::Bool) {
        cooked.asset.explicit_features[key] = value.boolean;
      }
    }
  }
  if (const Json *textures = root.find("textures");
      textures != nullptr && textures->kind == Json::Kind::Array) {
    for (const Json &texture_json : textures->array) {
      CookedMaterialTextureRecord texture;
      texture.role = textOr(texture_json, "role");
      texture.source_path = textOr(texture_json, "source_path");
      texture.cooked_path =
          std::filesystem::absolute(resolveRelative(root_path.parent_path(),
                                                    textOr(texture_json, "cooked_path")));
      texture.kind = textOr(texture_json, "kind");
      texture.color_space = textOr(texture_json, "color_space");
      texture.width = u32Or(texture_json, "width");
      texture.height = u32Or(texture_json, "height");
      texture.mip_count = u32Or(texture_json, "mip_count");
      texture.source_hash = textOr(texture_json, "source_hash");
      texture.cooked_hash = textOr(texture_json, "cooked_hash");
      if (const Json *diagnostics = texture_json.find("diagnostics");
          diagnostics != nullptr && diagnostics->kind == Json::Kind::Array) {
        for (const Json &diagnostic : diagnostics->array) {
          if (diagnostic.kind == Json::Kind::String) {
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
