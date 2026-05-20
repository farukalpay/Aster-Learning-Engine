// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_asset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
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

std::string normalizedName(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::replace(value.begin(), value.end(), '_', '-');
  return value;
}

std::optional<MaterialSurfaceProfile> surfaceProfileForName(const std::string &raw_value) {
  const std::string value = normalizedName(raw_value);
  if (value == "auto") {
    return MaterialSurfaceProfile::Auto;
  }
  if (value == "plain" || value == "none") {
    return MaterialSurfaceProfile::Plain;
  }
  if (value == "masonry" || value == "course-cells" || value == "weathered-stone") {
    return MaterialSurfaceProfile::Masonry;
  }
  if (value == "organic-fiber" || value == "fiber-strands" || value == "fur-fibers" ||
      value == "twig-nest") {
    return MaterialSurfaceProfile::OrganicFiber;
  }
  if (value == "biological-integument" || value == "integument" || value == "skin-fur" ||
      value == "fur-skin" || value == "dermal-fur") {
    return MaterialSurfaceProfile::BiologicalIntegument;
  }
  if (value == "terrain-layer" || value == "grass-soil" || value == "soil-path" ||
      value == "terrain-blend" || value == "layered-terrain") {
    return MaterialSurfaceProfile::TerrainLayer;
  }
  if (value == "liquid" || value == "water-surface") {
    return MaterialSurfaceProfile::Liquid;
  }
  if (value == "foliage") {
    return MaterialSurfaceProfile::Foliage;
  }
  if (value == "resin" || value == "amber-resin") {
    return MaterialSurfaceProfile::Resin;
  }
  if (value == "painted-wood" || value == "wood") {
    return MaterialSurfaceProfile::PaintedWood;
  }
  if (value == "feather" || value == "feather-vanes") {
    return MaterialSurfaceProfile::Feather;
  }
  if (value == "scales" || value == "iridescent-scales" || value == "reptile-scales") {
    return MaterialSurfaceProfile::Scales;
  }
  if (value == "stratified-rock" || value == "cave-rock" || value == "rock") {
    return MaterialSurfaceProfile::StratifiedRock;
  }
  if (value == "mineral-vein" || value == "coal-vein" || value == "ore-vein") {
    return MaterialSurfaceProfile::MineralVein;
  }
  if (value == "contact-shadow") {
    return MaterialSurfaceProfile::ContactShadow;
  }
  if (value == "filament-web" || value == "cave-web" || value == "web") {
    return MaterialSurfaceProfile::FilamentWeb;
  }
  if (value == "chitin-shell" || value == "cave-skitter-chitin" || value == "chitin") {
    return MaterialSurfaceProfile::ChitinShell;
  }
  if (value == "emissive-lens" || value == "cave-skitter-eye" || value == "glowing-eye") {
    return MaterialSurfaceProfile::EmissiveLens;
  }
  if (value == "corroded-metal" || value == "weathered-metal" || value == "rusted-metal") {
    return MaterialSurfaceProfile::CorrodedMetal;
  }
  if (value == "weld-bead" || value == "weld") {
    return MaterialSurfaceProfile::WeldBead;
  }
  return std::nullopt;
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
    const Token token = current_;
    const std::string value = readValueText();
    if (value == "true") {
      return true;
    }
    if (value == "false") {
      return false;
    }
    addError(token, "expected boolean value");
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
    if (field.text == "provenance") {
      parseStringMapBlock(asset.provenance, "provenance");
      return;
    }
    if (field.text == "authoring") {
      parseStringMapBlock(asset.authoring, "authoring");
      return;
    }
    if (field.text == "preview") {
      parseStringMapBlock(asset.preview, "preview");
      return;
    }
    if (field.text == "quality_profile") {
      parseStringMapBlock(asset.quality_profile, "quality_profile");
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
    } else if (field.text == "surface_profile") {
      const std::optional<MaterialSurfaceProfile> profile = surfaceProfileForName(value);
      if (profile.has_value()) {
        asset.surface_profile = *profile;
      } else {
        addError(field, "unknown surface_profile '" + value + "'");
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
    } else if (field.text == "depth_layer") {
      if (value == "BaseSurface" || value == "base-surface" || value == "base_surface" ||
          value == "base") {
        asset.depth_policy.layer = RenderDepthLayer::BaseSurface;
      } else if (value == "SurfaceAttachment" || value == "surface-attachment" ||
                 value == "surface_attachment") {
        asset.depth_policy.layer = RenderDepthLayer::SurfaceAttachment;
      } else if (value == "Decal" || value == "decal") {
        asset.depth_policy.layer = RenderDepthLayer::Decal;
      } else if (value == "ContactShadow" || value == "contact-shadow" ||
                 value == "contact_shadow") {
        asset.depth_policy.layer = RenderDepthLayer::ContactShadow;
      } else if (value == "DebugOverlay" || value == "debug-overlay" ||
                 value == "debug_overlay") {
        asset.depth_policy.layer = RenderDepthLayer::DebugOverlay;
      } else {
        addError(field, "unknown depth_layer '" + value + "'");
      }
    } else if (field.text == "depth_bias") {
      asset.depth_policy.constant_bias = std::stof(value);
    } else if (field.text == "slope_depth_bias") {
      asset.depth_policy.slope_bias = std::stof(value);
    } else if (field.text == "normal_offset") {
      asset.depth_policy.normal_offset = std::stof(value);
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

  void parseStringMapBlock(std::map<std::string, std::string> &values,
                           const std::string_view block_name) {
    expect(TokenKind::LeftBrace, "expected '{' after " + std::string(block_name));
    while (current_.kind != TokenKind::RightBrace && current_.kind != TokenKind::End) {
      if (current_.kind != TokenKind::Identifier) {
        addError(current_, "expected metadata key");
        advance();
        continue;
      }
      const std::string name = current_.text;
      advance();
      expect(TokenKind::Colon, "expected ':' after metadata key");
      values[name] = readValueText();
    }
    expect(TokenKind::RightBrace, "expected '}' after " + std::string(block_name));
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

std::string escapedString(const std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2u);
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

std::string serializedFloat(const float value) {
  std::ostringstream out;
  out << std::setprecision(7) << value;
  return out.str();
}

std::string_view materialSurfaceProfileAssetName(const MaterialSurfaceProfile profile) {
  switch (profile) {
  case MaterialSurfaceProfile::Auto:
    return "auto";
  case MaterialSurfaceProfile::Plain:
    return "plain";
  case MaterialSurfaceProfile::Masonry:
    return "masonry";
  case MaterialSurfaceProfile::OrganicFiber:
    return "organic-fiber";
  case MaterialSurfaceProfile::TerrainLayer:
    return "terrain-layer";
  case MaterialSurfaceProfile::Liquid:
    return "liquid";
  case MaterialSurfaceProfile::Foliage:
    return "foliage";
  case MaterialSurfaceProfile::Resin:
    return "resin";
  case MaterialSurfaceProfile::PaintedWood:
    return "painted-wood";
  case MaterialSurfaceProfile::Feather:
    return "feather";
  case MaterialSurfaceProfile::Scales:
    return "scales";
  case MaterialSurfaceProfile::StratifiedRock:
    return "stratified-rock";
  case MaterialSurfaceProfile::MineralVein:
    return "mineral-vein";
  case MaterialSurfaceProfile::ContactShadow:
    return "contact-shadow";
  case MaterialSurfaceProfile::FilamentWeb:
    return "filament-web";
  case MaterialSurfaceProfile::ChitinShell:
    return "chitin-shell";
  case MaterialSurfaceProfile::EmissiveLens:
    return "emissive-lens";
  case MaterialSurfaceProfile::CorrodedMetal:
    return "corroded-metal";
  case MaterialSurfaceProfile::WeldBead:
    return "weld-bead";
  case MaterialSurfaceProfile::BiologicalIntegument:
    return "biological-integument";
  }
  return "auto";
}

void appendStringMap(std::ostringstream &out, const std::string_view name,
                     const std::map<std::string, std::string> &values) {
  if (values.empty()) {
    return;
  }
  out << "\n  " << name << " {\n";
  for (const auto &[key, value] : values) {
    out << "    " << key << ": " << escapedString(value) << "\n";
  }
  out << "  }\n";
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

std::string serializeMaterialAsset(const MaterialAsset &asset) {
  std::ostringstream out;
  out << "material " << asset.id << " {\n";
  out << "  schema_version: " << asset.schema_version << "\n";
  if (!asset.name.empty()) {
    out << "  name: " << escapedString(asset.name) << "\n";
  }
  out << "  shading_model: " << materialShadingModelName(asset.shading_model) << "\n";
  if (asset.surface_profile != MaterialSurfaceProfile::Auto) {
    out << "  surface_profile: " << materialSurfaceProfileAssetName(asset.surface_profile)
        << "\n";
  }
  out << "  blend_mode: " << materialBlendModeName(asset.blend_mode) << "\n";
  out << "  cull_mode: " << materialAssetCullModeName(asset.cull_mode) << "\n";
  out << "  receives_decals: " << (asset.receives_decals ? "true" : "false") << "\n";
  out << "  receives_shadows: " << (asset.receives_shadows ? "true" : "false") << "\n";

  appendStringMap(out, "provenance", asset.provenance);
  appendStringMap(out, "authoring", asset.authoring);
  appendStringMap(out, "preview", asset.preview);
  appendStringMap(out, "quality_profile", asset.quality_profile);

  if (!asset.textures.empty()) {
    out << "\n  textures {\n";
    for (const auto &[role, slot] : asset.textures) {
      out << "    " << role << ": " << escapedString(slot.uri.generic_string()) << "\n";
    }
    out << "  }\n";
  }

  if (!asset.params.empty()) {
    out << "\n  params {\n";
    for (const auto &[name, value] : asset.params) {
      out << "    " << name << ": " << serializedFloat(value) << "\n";
    }
    out << "  }\n";
  }

  if (!asset.explicit_features.empty()) {
    out << "\n  features {\n";
    for (const auto &[name, value] : asset.explicit_features) {
      out << "    " << name << ": " << (value ? "true" : "false") << "\n";
    }
    out << "  }\n";
  }

  if (!asset.layers.empty()) {
    out << "\n  layers {\n";
    for (const MaterialLayerExpression &layer : asset.layers) {
      out << "    " << layer.name << ": "
          << (layer.raw.empty() ? layer.operation : layer.raw) << "\n";
    }
    out << "  }\n";
  }

  out << "}\n";
  return out.str();
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
  desc.base_color = LinearRgb{paramOr(asset, "base_color_r", 1.0f),
                              paramOr(asset, "base_color_g", 1.0f),
                              paramOr(asset, "base_color_b", 1.0f)};
  desc.roughness = paramOr(asset, "roughness", paramOr(asset, "base_roughness", desc.roughness));
  desc.metallic = paramOr(asset, "metallic", desc.metallic);
  desc.opacity = paramOr(asset, "opacity", desc.opacity);
  desc.emission_strength = paramOr(asset, "emission_strength", desc.emission_strength);
  desc.emission_color = EmissionColor{paramOr(asset, "emission_r", desc.emission_color.x),
                                      paramOr(asset, "emission_g", desc.emission_color.y),
                                      paramOr(asset, "emission_b", desc.emission_color.z)};
  desc.detail_scale = paramOr(asset, "triplanar_scale", paramOr(asset, "detail_scale", 1.0f));
  desc.detail_strength = paramOr(asset, "detail_strength", 0.0f);
  desc.edge_wear = paramOr(asset, "edge_wear", paramOr(asset, "edge_polish", desc.edge_wear));
  desc.ambient_occlusion = paramOr(asset, "ambient_occlusion", paramOr(asset, "ao", 1.0f));
  desc.procedural.wetness = paramOr(asset, "wetness_strength", paramOr(asset, "wetness", 0.0f));
  desc.procedural.macro_variation = paramOr(asset, "macro_variation", 0.0f);
  desc.procedural.micro_normal_strength =
      paramOr(asset, "micro_normal_strength", materialFeatureSet(asset).normal_map ? 0.28f : 0.0f);
  desc.procedural.roughness_variation = paramOr(asset, "roughness_variation", 0.0f);
  desc.procedural.physical_texel_density =
      paramOr(asset, "physical_texel_density", paramOr(asset, "texel_density", 512.0f));
  desc.procedural.height_normal_coupling =
      paramOr(asset, "height_normal_coupling", materialFeatureSet(asset).height ? 0.82f : 0.0f);
  desc.procedural.roughness_height_coupling =
      paramOr(asset, "roughness_height_coupling", materialFeatureSet(asset).height ? 0.56f : 0.0f);
  desc.procedural.macro_frequency_breakup =
      paramOr(asset, "macro_frequency_breakup", desc.procedural.macro_variation * 0.45f);
  desc.procedural.micro_frequency_breakup =
      paramOr(asset, "micro_frequency_breakup", desc.procedural.micro_normal_strength * 0.60f);
  desc.procedural.height_shading = paramOr(asset, "height_shading", materialFeatureSet(asset).height ? 0.18f : 0.0f);
  desc.procedural.pitting_density = paramOr(asset, "pitting_density", 0.0f);
  desc.procedural.pitting_depth = paramOr(asset, "pitting_depth", 0.0f);
  desc.procedural.oxide_layering = paramOr(asset, "oxide_layering", 0.0f);
  desc.procedural.cavity_grime = paramOr(asset, "cavity_grime", paramOr(asset, "seam_grime", 0.0f));
  desc.procedural.edge_polish = paramOr(asset, "edge_polish", paramOr(asset, "exposed_edge_ratio", 0.0f));
  desc.procedural.weld_heat_tint = paramOr(asset, "weld_heat_tint", 0.0f);
  desc.procedural.axial_scratches = paramOr(asset, "axial_scratches", 0.0f);
  desc.procedural.wet_streaks = paramOr(asset, "wet_streaks", 0.0f);
  desc.procedural.rust_bloom = paramOr(asset, "rust_bloom", 0.0f);
  desc.procedural.black_scab = paramOr(asset, "black_scab", 0.0f);
  desc.procedural.paint_remnant = paramOr(asset, "paint_remnant", 0.0f);
  desc.procedural.weld_slag = paramOr(asset, "weld_slag", 0.0f);
  desc.procedural.rim_soot = paramOr(asset, "rim_soot", 0.0f);
  desc.pattern_scale = {desc.detail_scale, desc.detail_scale};
  desc.pattern_depth = paramOr(asset, "pattern_depth", desc.procedural.height_shading);
  desc.pattern_contrast = paramOr(asset, "pattern_contrast", 0.0f);
  desc.pattern_mortar = paramOr(asset, "pattern_mortar", 0.08f);
  desc.depth_policy = asset.depth_policy;
  desc.receives_shadows = asset.receives_shadows;
  desc.surface_profile = asset.surface_profile;
  desc.procedural_graph_guid = asset.procedural_graph_guid;
  desc.procedural_graph_node = asset.procedural_graph_node;
  desc.procedural_capability_status = asset.procedural_capability_status;
  desc.procedural_pipeline_key = asset.procedural_pipeline_key;
  if (desc.surface_profile == MaterialSurfaceProfile::Auto &&
      (materialFeatureSet(asset).triplanar || materialFeatureSet(asset).height)) {
    desc.surface_profile = MaterialSurfaceProfile::StratifiedRock;
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
  material.shader_variant_key = asset.procedural_shader_variant_key == 0u
                                    ? materialFeatureMask(materialFeatureSet(asset))
                                    : asset.procedural_shader_variant_key;
  return material;
}

} // namespace aster
