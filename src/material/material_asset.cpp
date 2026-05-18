// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_asset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>

namespace aster {
namespace {

enum class TokenKind {
  Identifier,
  String,
  Number,
  LeftBrace,
  RightBrace,
  LeftParen,
  RightParen,
  Colon,
  Comma,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  std::size_t line = 1u;
  std::size_t column = 1u;
};

bool isIdentifierStart(const char value) {
  return std::isalpha(static_cast<unsigned char>(value)) || value == '_';
}

bool isIdentifierBody(const char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '_' || value == '-' ||
         value == '.';
}

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("could not open material asset: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class Lexer {
public:
  Lexer(std::string_view source, std::filesystem::path path,
        std::vector<MaterialDiagnostic> &diagnostics)
      : source_(source), path_(std::move(path)), diagnostics_(diagnostics) {}

  [[nodiscard]] Token next() {
    skipTrivia();
    Token token;
    token.line = line_;
    token.column = column_;
    if (position_ >= source_.size()) {
      token.kind = TokenKind::End;
      return token;
    }

    const char c = source_[position_];
    switch (c) {
    case '{':
      advance();
      token.kind = TokenKind::LeftBrace;
      token.text = "{";
      return token;
    case '}':
      advance();
      token.kind = TokenKind::RightBrace;
      token.text = "}";
      return token;
    case '(':
      advance();
      token.kind = TokenKind::LeftParen;
      token.text = "(";
      return token;
    case ')':
      advance();
      token.kind = TokenKind::RightParen;
      token.text = ")";
      return token;
    case ':':
      advance();
      token.kind = TokenKind::Colon;
      token.text = ":";
      return token;
    case ',':
      advance();
      token.kind = TokenKind::Comma;
      token.text = ",";
      return token;
    case '"':
      return stringToken();
    default:
      break;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+') {
      return numberToken();
    }
    if (isIdentifierStart(c)) {
      return identifierToken();
    }

    addError(token, std::string("unexpected character '") + c + "'");
    advance();
    return next();
  }

private:
  void advance() {
    if (position_ >= source_.size()) {
      return;
    }
    if (source_[position_] == '\n') {
      ++line_;
      column_ = 1u;
    } else {
      ++column_;
    }
    ++position_;
  }

  void skipTrivia() {
    for (;;) {
      while (position_ < source_.size() &&
             std::isspace(static_cast<unsigned char>(source_[position_]))) {
        advance();
      }
      if (position_ + 1u < source_.size() && source_[position_] == '/' &&
          source_[position_ + 1u] == '/') {
        while (position_ < source_.size() && source_[position_] != '\n') {
          advance();
        }
        continue;
      }
      if (position_ < source_.size() && source_[position_] == '#') {
        while (position_ < source_.size() && source_[position_] != '\n') {
          advance();
        }
        continue;
      }
      break;
    }
  }

  [[nodiscard]] Token identifierToken() {
    Token token{.kind = TokenKind::Identifier, .line = line_, .column = column_};
    while (position_ < source_.size() && isIdentifierBody(source_[position_])) {
      token.text.push_back(source_[position_]);
      advance();
    }
    return token;
  }

  [[nodiscard]] Token numberToken() {
    Token token{.kind = TokenKind::Number, .line = line_, .column = column_};
    if (source_[position_] == '-' || source_[position_] == '+') {
      token.text.push_back(source_[position_]);
      advance();
    }
    while (position_ < source_.size() &&
           (std::isdigit(static_cast<unsigned char>(source_[position_])) ||
            source_[position_] == '.' || source_[position_] == 'e' || source_[position_] == 'E' ||
            source_[position_] == '-' || source_[position_] == '+')) {
      token.text.push_back(source_[position_]);
      advance();
      if (!token.text.empty() &&
          (token.text.back() == '-' || token.text.back() == '+') &&
          token.text.size() > 1u && token.text[token.text.size() - 2u] != 'e' &&
          token.text[token.text.size() - 2u] != 'E') {
        break;
      }
    }
    return token;
  }

  [[nodiscard]] Token stringToken() {
    Token token{.kind = TokenKind::String, .line = line_, .column = column_};
    advance();
    while (position_ < source_.size()) {
      const char c = source_[position_];
      if (c == '"') {
        advance();
        return token;
      }
      if (c == '\\') {
        advance();
        if (position_ >= source_.size()) {
          break;
        }
        const char escaped = source_[position_];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
          token.text.push_back(escaped);
          break;
        case 'n':
          token.text.push_back('\n');
          break;
        case 't':
          token.text.push_back('\t');
          break;
        default:
          token.text.push_back(escaped);
          break;
        }
        advance();
        continue;
      }
      token.text.push_back(c);
      advance();
    }
    addError(token, "unterminated string literal");
    return token;
  }

  void addError(const Token &token, std::string message) {
    diagnostics_.push_back({.severity = MaterialDiagnosticSeverity::Error,
                            .source_path = path_,
                            .line = token.line,
                            .column = token.column,
                            .message = std::move(message)});
  }

  std::string_view source_;
  std::filesystem::path path_;
  std::vector<MaterialDiagnostic> &diagnostics_;
  std::size_t position_ = 0u;
  std::size_t line_ = 1u;
  std::size_t column_ = 1u;
};

class Parser {
public:
  Parser(std::string_view source, std::filesystem::path path,
         std::vector<MaterialDiagnostic> &diagnostics)
      : lexer_(source, path, diagnostics), path_(std::move(path)), diagnostics_(diagnostics) {
    advance();
  }

  [[nodiscard]] MaterialAsset parse() {
    MaterialAsset asset;
    asset.source_path = path_;
    expectIdentifier("material");
    if (current_.kind == TokenKind::Identifier) {
      asset.id = current_.text;
      asset.name = current_.text;
      advance();
    } else {
      addError(current_, "expected material identifier");
    }
    expect(TokenKind::LeftBrace, "expected '{' after material name");
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      parseTopLevel(asset);
    }
    expect(TokenKind::RightBrace, "expected '}' after material body");
    if (current_.kind != TokenKind::End) {
      addError(current_, "unexpected tokens after material body");
    }
    return asset;
  }

private:
  void advance() {
    current_ = lexer_.next();
  }

  bool expect(const TokenKind kind, std::string message) {
    if (current_.kind == kind) {
      advance();
      return true;
    }
    addError(current_, std::move(message));
    return false;
  }

  bool expectIdentifier(const std::string_view value) {
    if (current_.kind == TokenKind::Identifier && current_.text == value) {
      advance();
      return true;
    }
    addError(current_, "expected '" + std::string(value) + "'");
    return false;
  }

  [[nodiscard]] std::string readValueText() {
    if (current_.kind == TokenKind::Identifier || current_.kind == TokenKind::String ||
        current_.kind == TokenKind::Number) {
      std::string out = current_.text;
      advance();
      return out;
    }
    addError(current_, "expected value");
    return {};
  }

  [[nodiscard]] std::optional<float> readFloat() {
    if (current_.kind != TokenKind::Number) {
      addError(current_, "expected numeric value");
      return std::nullopt;
    }
    try {
      const float value = std::stof(current_.text);
      advance();
      return value;
    } catch (const std::exception &) {
      addError(current_, "invalid numeric value '" + current_.text + "'");
      advance();
      return std::nullopt;
    }
  }

  [[nodiscard]] bool readBool() {
    const std::string value = readValueText();
    if (value == "true") {
      return true;
    }
    if (value == "false") {
      return false;
    }
    addError(current_, "expected boolean value");
    return false;
  }

  void parseTopLevel(MaterialAsset &asset) {
    if (current_.kind != TokenKind::Identifier) {
      addError(current_, "expected material field");
      advance();
      return;
    }
    const Token field = current_;
    advance();
    if (field.text == "textures") {
      parseTextureBlock(asset);
      return;
    }
    if (field.text == "params") {
      parseParamBlock(asset);
      return;
    }
    if (field.text == "features") {
      parseFeatureBlock(asset);
      return;
    }
    if (field.text == "layers") {
      parseLayerBlock(asset);
      return;
    }
    expect(TokenKind::Colon, "expected ':' after material field");
    const std::string value = readValueText();
    if (field.text == "schema_version") {
      asset.schema_version = static_cast<std::uint32_t>(std::max(0, std::stoi(value)));
    } else if (field.text == "name") {
      asset.name = value;
    } else if (field.text == "shading_model") {
      if (value == "LitPBR" || value == "lit_pbr") {
        asset.shading_model = MaterialShadingModel::LitPBR;
      } else if (value == "Unlit" || value == "unlit") {
        asset.shading_model = MaterialShadingModel::Unlit;
      } else if (value == "Emissive" || value == "emissive") {
        asset.shading_model = MaterialShadingModel::Emissive;
      } else {
        addError(field, "unknown shading_model '" + value + "'");
      }
    } else if (field.text == "blend_mode") {
      if (value == "Opaque" || value == "opaque") {
        asset.blend_mode = MaterialBlendMode::Opaque;
      } else if (value == "Masked" || value == "masked" || value == "AlphaClip") {
        asset.blend_mode = MaterialBlendMode::Masked;
      } else if (value == "Blend" || value == "blend") {
        asset.blend_mode = MaterialBlendMode::Blend;
      } else {
        addError(field, "unknown blend_mode '" + value + "'");
      }
    } else if (field.text == "cull_mode") {
      if (value == "Back" || value == "back") {
        asset.cull_mode = MaterialAssetCullMode::Back;
      } else if (value == "Front" || value == "front") {
        asset.cull_mode = MaterialAssetCullMode::Front;
      } else if (value == "None" || value == "none") {
        asset.cull_mode = MaterialAssetCullMode::None;
      } else {
        addError(field, "unknown cull_mode '" + value + "'");
      }
    } else if (field.text == "receives_decals") {
      asset.receives_decals = value == "true";
    } else if (field.text == "receives_shadows") {
      asset.receives_shadows = value != "false";
    } else {
      addError(field, "unknown material field '" + field.text + "'");
    }
  }

  void parseTextureBlock(MaterialAsset &asset) {
    expect(TokenKind::LeftBrace, "expected '{' after textures");
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      if (current_.kind != TokenKind::Identifier) {
        addError(current_, "expected texture slot name");
        advance();
        continue;
      }
      const std::string role = current_.text;
      advance();
      expect(TokenKind::Colon, "expected ':' after texture slot");
      const std::filesystem::path uri = readValueText();
      MaterialTextureSlot slot;
      slot.role = role;
      slot.uri = uri;
      slot.srgb = role == "albedo" || role == "base_color" || role == "emissive";
      asset.textures[role] = std::move(slot);
    }
    expect(TokenKind::RightBrace, "expected '}' after textures");
  }

  void parseParamBlock(MaterialAsset &asset) {
    expect(TokenKind::LeftBrace, "expected '{' after params");
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      if (current_.kind != TokenKind::Identifier) {
        addError(current_, "expected parameter name");
        advance();
        continue;
      }
      const std::string name = current_.text;
      advance();
      expect(TokenKind::Colon, "expected ':' after parameter name");
      if (const std::optional<float> value = readFloat()) {
        asset.params[name] = *value;
      }
    }
    expect(TokenKind::RightBrace, "expected '}' after params");
  }

  void parseFeatureBlock(MaterialAsset &asset) {
    expect(TokenKind::LeftBrace, "expected '{' after features");
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      if (current_.kind != TokenKind::Identifier) {
        addError(current_, "expected feature name");
        advance();
        continue;
      }
      const std::string name = current_.text;
      advance();
      expect(TokenKind::Colon, "expected ':' after feature name");
      asset.explicit_features[name] = readBool();
    }
    expect(TokenKind::RightBrace, "expected '}' after features");
  }

  void parseLayerBlock(MaterialAsset &asset) {
    expect(TokenKind::LeftBrace, "expected '{' after layers");
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      if (current_.kind != TokenKind::Identifier) {
        addError(current_, "expected layer name");
        advance();
        continue;
      }
      MaterialLayerExpression layer;
      layer.name = current_.text;
      advance();
      expect(TokenKind::Colon, "expected ':' after layer name");
      if (current_.kind == TokenKind::Identifier) {
        layer.operation = current_.text;
        layer.raw = current_.text;
        advance();
      } else {
        addError(current_, "expected layer operation");
      }
      expect(TokenKind::LeftParen, "expected '(' after layer operation");
      layer.raw += "(";
      while (current_.kind != TokenKind::RightParen && current_.kind != TokenKind::End) {
        if (current_.kind == TokenKind::Comma) {
          layer.raw += ",";
          advance();
          continue;
        }
        const std::string argument = readValueText();
        if (!argument.empty()) {
          layer.arguments.push_back(argument);
          layer.raw += argument;
        }
      }
      expect(TokenKind::RightParen, "expected ')' after layer expression");
      layer.raw += ")";
      asset.layers.push_back(std::move(layer));
    }
    expect(TokenKind::RightBrace, "expected '}' after layers");
  }

  void addError(const Token &token, std::string message) {
    diagnostics_.push_back({.severity = MaterialDiagnosticSeverity::Error,
                            .source_path = path_,
                            .line = token.line,
                            .column = token.column,
                            .message = std::move(message)});
  }

  Lexer lexer_;
  std::filesystem::path path_;
  std::vector<MaterialDiagnostic> &diagnostics_;
  Token current_{};
};

bool hasTexture(const MaterialAsset &asset, const std::string_view role) {
  return asset.textures.find(std::string(role)) != asset.textures.end();
}

bool hasFeature(const MaterialAsset &asset, const std::string_view feature) {
  const auto it = asset.explicit_features.find(std::string(feature));
  return it != asset.explicit_features.end() && it->second;
}

bool hasLayerOp(const MaterialAsset &asset, const std::string_view operation) {
  return std::any_of(asset.layers.begin(), asset.layers.end(),
                     [&](const MaterialLayerExpression &layer) {
                       return layer.operation == operation;
                     });
}

float paramOr(const MaterialAsset &asset, const std::string_view name, const float fallback) {
  const auto it = asset.params.find(std::string(name));
  return it == asset.params.end() ? fallback : it->second;
}

} // namespace

bool MaterialAssetLoadResult::ok() const {
  return std::none_of(diagnostics.begin(), diagnostics.end(), [](const MaterialDiagnostic &diag) {
    return diag.severity == MaterialDiagnosticSeverity::Error;
  });
}

std::string_view materialShadingModelName(const MaterialShadingModel value) {
  switch (value) {
  case MaterialShadingModel::LitPBR:
    return "LitPBR";
  case MaterialShadingModel::Unlit:
    return "Unlit";
  case MaterialShadingModel::Emissive:
    return "Emissive";
  }
  return "LitPBR";
}

std::string_view materialBlendModeName(const MaterialBlendMode value) {
  switch (value) {
  case MaterialBlendMode::Opaque:
    return "Opaque";
  case MaterialBlendMode::Masked:
    return "Masked";
  case MaterialBlendMode::Blend:
    return "Blend";
  }
  return "Opaque";
}

std::string_view materialAssetCullModeName(const MaterialAssetCullMode value) {
  switch (value) {
  case MaterialAssetCullMode::None:
    return "None";
  case MaterialAssetCullMode::Back:
    return "Back";
  case MaterialAssetCullMode::Front:
    return "Front";
  }
  return "Back";
}

MaterialAssetLoadResult parseMaterialAsset(const std::string_view source,
                                           std::filesystem::path source_path) {
  MaterialAssetLoadResult result;
  Parser parser(source, source_path, result.diagnostics);
  result.value = parser.parse();
  std::vector<MaterialDiagnostic> validation = validateMaterialAsset(result.value);
  result.diagnostics.insert(result.diagnostics.end(), validation.begin(), validation.end());
  return result;
}

MaterialAssetLoadResult loadMaterialAsset(const std::filesystem::path &path) {
  try {
    return parseMaterialAsset(readTextFile(path), path);
  } catch (const std::exception &error) {
    MaterialAssetLoadResult result;
    result.value.source_path = path;
    result.diagnostics.push_back({.severity = MaterialDiagnosticSeverity::Error,
                                  .source_path = path,
                                  .message = error.what()});
    return result;
  }
}

std::vector<MaterialDiagnostic> validateMaterialAsset(const MaterialAsset &asset) {
  std::vector<MaterialDiagnostic> diagnostics;
  const auto add_error = [&](std::string message) {
    diagnostics.push_back({.severity = MaterialDiagnosticSeverity::Error,
                           .source_path = asset.source_path,
                           .message = std::move(message)});
  };
  if (asset.id.empty()) {
    add_error("material asset is missing an id");
  }
  if (asset.shading_model == MaterialShadingModel::LitPBR &&
      !hasTexture(asset, "albedo") && !hasTexture(asset, "base_color")) {
    diagnostics.push_back({.severity = MaterialDiagnosticSeverity::Warning,
                           .source_path = asset.source_path,
                           .message = "LitPBR material has no albedo/base_color texture"});
  }
  for (const MaterialLayerExpression &layer : asset.layers) {
    if (layer.operation != "triplanar" && layer.operation != "height_blend" &&
        layer.operation != "slope_blend" && layer.operation != "wetness" &&
        layer.operation != "moss") {
      add_error("unsupported layer operation '" + layer.operation + "'");
    }
  }
  return diagnostics;
}

MaterialFeatureSet materialFeatureSet(const MaterialAsset &asset) {
  MaterialFeatureSet features;
  features.textured = !asset.textures.empty();
  features.normal_map = hasTexture(asset, "normal") || hasFeature(asset, "normal_map");
  features.orm_texture = hasTexture(asset, "orm") || hasTexture(asset, "metallic_roughness") ||
                         hasTexture(asset, "roughness") || hasTexture(asset, "metallic") ||
                         hasTexture(asset, "ao");
  features.emissive = hasTexture(asset, "emissive") || asset.shading_model == MaterialShadingModel::Emissive ||
                      paramOr(asset, "emission_strength", 0.0f) > 0.0f;
  features.height = hasTexture(asset, "height") || hasFeature(asset, "height");
  features.parallax = hasFeature(asset, "parallax") || hasFeature(asset, "parallax_occlusion");
  features.triplanar = hasFeature(asset, "triplanar") || hasLayerOp(asset, "triplanar");
  features.decal_receiver = asset.receives_decals || hasFeature(asset, "decal_receiver");
  features.fog = !hasFeature(asset, "disable_fog");
  features.shadow = asset.receives_shadows;
  features.alpha_clip = asset.blend_mode == MaterialBlendMode::Masked;
  features.alpha_blend = asset.blend_mode == MaterialBlendMode::Blend;
  features.double_sided = asset.cull_mode == MaterialAssetCullMode::None;
  features.instancing = !hasFeature(asset, "disable_instancing");
  return features;
}

std::uint64_t materialFeatureMask(const MaterialFeatureSet &features) {
  std::uint64_t mask = 0u;
  const auto set = [&](const bool enabled, const std::uint64_t bit) {
    if (enabled) {
      mask |= bit;
    }
  };
  set(features.textured, 1ull << 0ull);
  set(features.normal_map, 1ull << 1ull);
  set(features.orm_texture, 1ull << 2ull);
  set(features.emissive, 1ull << 3ull);
  set(features.height, 1ull << 4ull);
  set(features.parallax, 1ull << 5ull);
  set(features.triplanar, 1ull << 6ull);
  set(features.decal_receiver, 1ull << 7ull);
  set(features.fog, 1ull << 8ull);
  set(features.shadow, 1ull << 9ull);
  set(features.alpha_clip, 1ull << 10ull);
  set(features.alpha_blend, 1ull << 11ull);
  set(features.double_sided, 1ull << 12ull);
  set(features.instancing, 1ull << 13ull);
  return mask;
}

Material resolveMaterialAssetFallback(const MaterialAsset &asset) {
  MaterialDesc desc;
  desc.base_color = {paramOr(asset, "base_color_r", 1.0f), paramOr(asset, "base_color_g", 1.0f),
                     paramOr(asset, "base_color_b", 1.0f)};
  desc.roughness = paramOr(asset, "roughness", paramOr(asset, "base_roughness", desc.roughness));
  desc.metallic = paramOr(asset, "metallic", desc.metallic);
  desc.opacity = paramOr(asset, "opacity", desc.opacity);
  desc.emission_strength = paramOr(asset, "emission_strength", desc.emission_strength);
  desc.emission_color = {paramOr(asset, "emission_r", desc.emission_color.x),
                         paramOr(asset, "emission_g", desc.emission_color.y),
                         paramOr(asset, "emission_b", desc.emission_color.z)};
  desc.detail_scale = paramOr(asset, "triplanar_scale", paramOr(asset, "detail_scale", 1.0f));
  desc.detail_strength = paramOr(asset, "detail_strength", 0.0f);
  desc.ambient_occlusion = paramOr(asset, "ambient_occlusion", paramOr(asset, "ao", 1.0f));
  desc.procedural.wetness = paramOr(asset, "wetness_strength", paramOr(asset, "wetness", 0.0f));
  desc.procedural.macro_variation = paramOr(asset, "macro_variation", 0.0f);
  desc.procedural.micro_normal_strength =
      paramOr(asset, "micro_normal_strength", materialFeatureSet(asset).normal_map ? 0.28f : 0.0f);
  desc.procedural.roughness_variation = paramOr(asset, "roughness_variation", 0.0f);
  desc.procedural.height_shading = paramOr(asset, "height_shading", materialFeatureSet(asset).height ? 0.18f : 0.0f);
  desc.pattern_scale = {desc.detail_scale, desc.detail_scale};
  desc.pattern_depth = paramOr(asset, "pattern_depth", desc.procedural.height_shading);
  desc.pattern_contrast = paramOr(asset, "pattern_contrast", 0.0f);
  desc.pattern_mortar = paramOr(asset, "pattern_mortar", 0.08f);
  if (materialFeatureSet(asset).triplanar || asset.id.find("cave") != std::string::npos) {
    desc.surface_pattern = SurfacePattern::CaveRock;
  }
  desc.double_sided = asset.cull_mode == MaterialAssetCullMode::None;
  switch (asset.cull_mode) {
  case MaterialAssetCullMode::None:
    desc.cull_mode = FaceCullMode::None;
    break;
  case MaterialAssetCullMode::Back:
    desc.cull_mode = FaceCullMode::Back;
    break;
  case MaterialAssetCullMode::Front:
    desc.cull_mode = FaceCullMode::Front;
    break;
  }
  switch (asset.blend_mode) {
  case MaterialBlendMode::Opaque:
    desc.alpha_mode = MaterialAlphaMode::Opaque;
    break;
  case MaterialBlendMode::Masked:
    desc.alpha_mode = MaterialAlphaMode::Masked;
    break;
  case MaterialBlendMode::Blend:
    desc.alpha_mode = MaterialAlphaMode::Blend;
    desc.depth_write = MaterialDepthWrite::Disabled;
    break;
  }
  Material material = makeMaterial(desc);
  material.asset_id = asset.id;
  material.shader_variant_key = materialFeatureMask(materialFeatureSet(asset));
  return material;
}

} // namespace aster
