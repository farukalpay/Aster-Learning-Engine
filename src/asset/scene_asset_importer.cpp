// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/asset/scene_asset_importer.hpp"

#include "aster/math/mat4.hpp"
#include "aster/math/color.hpp"
#include "aster/render/material_compiler.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

  [[nodiscard]] const Json *find(const std::string &key) const {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
  }

  [[nodiscard]] const Json &at(const std::string &key) const {
    const Json *value = find(key);
    if (value == nullptr) {
      throw std::runtime_error("Scene asset is missing required JSON key: " + key);
    }
    return *value;
  }

  [[nodiscard]] const Json &at(const std::size_t index) const {
    if (index >= array.size()) {
      throw std::runtime_error("Scene asset JSON array index is out of range.");
    }
    return array[index];
  }

  [[nodiscard]] std::string_view text(const std::string_view fallback = {}) const {
    return kind == Kind::String ? std::string_view(string) : fallback;
  }

  [[nodiscard]] double numeric(const double fallback = 0.0) const {
    return kind == Kind::Number ? number : fallback;
  }

  [[nodiscard]] bool flag(const bool fallback = false) const {
    return kind == Kind::Bool ? boolean : fallback;
  }
};

class JsonParser {
public:
  explicit JsonParser(std::string_view source) : source_(source) {}

  Json parse() {
    Json value = parseValue();
    skipWhitespace();
    if (position_ != source_.size()) {
      throw std::runtime_error("Scene asset JSON has trailing data.");
    }
    return value;
  }

private:
  void skipWhitespace() {
    while (position_ < source_.size()) {
      const char c = source_[position_];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
        break;
      }
      ++position_;
    }
  }

  char peek() {
    skipWhitespace();
    if (position_ >= source_.size()) {
      throw std::runtime_error("Unexpected end of scene asset JSON.");
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
      throw std::runtime_error("Scene asset JSON has unexpected syntax.");
    }
  }

  Json parseValue() {
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
      position_ += 4;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = true;
      return value;
    }
    if (source_.substr(position_, 5) == "false") {
      position_ += 5;
      Json value;
      value.kind = Json::Kind::Bool;
      value.boolean = false;
      return value;
    }
    if (source_.substr(position_, 4) == "null") {
      position_ += 4;
      return {};
    }
    throw std::runtime_error("Scene asset JSON has an unsupported value.");
  }

  Json parseObject() {
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

  Json parseArray() {
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

  std::string parseString() {
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
        throw std::runtime_error("Scene asset JSON has an unterminated escape.");
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
        throw std::runtime_error("Scene asset JSON uses an unsupported string escape.");
      }
    }
    throw std::runtime_error("Scene asset JSON has an unterminated string.");
  }

  Json parseNumber() {
    const std::size_t start = position_;
    if (source_[position_] == '-') {
      ++position_;
    }
    while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
      ++position_;
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      ++position_;
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      ++position_;
      if (position_ < source_.size() && (source_[position_] == '-' || source_[position_] == '+')) {
        ++position_;
      }
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
        ++position_;
      }
    }

    double number = 0.0;
    const std::string token(source_.substr(start, position_ - start));
    const char *begin = token.data();
    char *end = nullptr;
    number = std::strtod(begin, &end);
    if (end != begin + static_cast<std::ptrdiff_t>(token.size())) {
      throw std::runtime_error("Scene asset JSON number could not be parsed.");
    }
    Json value;
    value.kind = Json::Kind::Number;
    value.number = number;
    return value;
  }

  std::string_view source_;
  std::size_t position_ = 0;
};

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open scene asset file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Could not open scene asset buffer: " + path.string());
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::size_t integer(const Json *value, const std::size_t fallback = 0u) {
  return value == nullptr ? fallback : static_cast<std::size_t>(value->numeric());
}

struct BufferView {
  std::size_t buffer = 0;
  std::size_t byte_offset = 0;
  std::size_t byte_length = 0;
  std::size_t byte_stride = 0;
};

struct Accessor {
  std::size_t buffer_view = 0;
  std::size_t byte_offset = 0;
  int component_type = 0;
  std::size_t count = 0;
  std::string type;
};

struct AssetData {
  Json root;
  std::vector<std::vector<std::uint8_t>> buffers;
  std::vector<BufferView> views;
  std::vector<Accessor> accessors;
};

std::size_t componentCount(const std::string &type) {
  if (type == "SCALAR") {
    return 1u;
  }
  if (type == "VEC2") {
    return 2u;
  }
  if (type == "VEC3") {
    return 3u;
  }
  if (type == "VEC4") {
    return 4u;
  }
  if (type == "MAT4") {
    return 16u;
  }
  throw std::runtime_error("Scene asset accessor uses an unsupported component shape.");
}

std::size_t componentSize(const int component_type) {
  switch (component_type) {
  case 5121:
    return 1u;
  case 5123:
    return 2u;
  case 5125:
  case 5126:
    return 4u;
  default:
    throw std::runtime_error("Scene asset accessor uses an unsupported component type.");
  }
}

template <typename T>
T readScalar(const std::vector<std::uint8_t> &bytes, const std::size_t offset) {
  if (offset + sizeof(T) > bytes.size()) {
    throw std::runtime_error("Scene asset accessor reads past the end of a buffer.");
  }
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

const Accessor &accessor(const AssetData &data, const std::size_t index) {
  if (index >= data.accessors.size()) {
    throw std::runtime_error("Scene asset references an invalid accessor.");
  }
  return data.accessors[index];
}

const BufferView &view(const AssetData &data, const std::size_t index) {
  if (index >= data.views.size()) {
    throw std::runtime_error("Scene asset references an invalid buffer view.");
  }
  return data.views[index];
}

float readFloatComponent(const AssetData &data, const Accessor &accessor,
                         const std::size_t element, const std::size_t component) {
  const BufferView &buffer_view = view(data, accessor.buffer_view);
  const std::vector<std::uint8_t> &buffer = data.buffers.at(buffer_view.buffer);
  const std::size_t stride =
      buffer_view.byte_stride == 0u
          ? componentCount(accessor.type) * componentSize(accessor.component_type)
          : buffer_view.byte_stride;
  const std::size_t offset = buffer_view.byte_offset + accessor.byte_offset + element * stride +
                             component * componentSize(accessor.component_type);
  if (accessor.component_type == 5126) {
    return readScalar<float>(buffer, offset);
  }
  if (accessor.component_type == 5121) {
    return static_cast<float>(readScalar<std::uint8_t>(buffer, offset));
  }
  if (accessor.component_type == 5123) {
    return static_cast<float>(readScalar<std::uint16_t>(buffer, offset));
  }
  if (accessor.component_type == 5125) {
    return static_cast<float>(readScalar<std::uint32_t>(buffer, offset));
  }
  throw std::runtime_error("Scene asset float accessor component type is unsupported.");
}

std::uint32_t readIndex(const AssetData &data, const Accessor &accessor, const std::size_t element) {
  const BufferView &buffer_view = view(data, accessor.buffer_view);
  const std::vector<std::uint8_t> &buffer = data.buffers.at(buffer_view.buffer);
  const std::size_t stride =
      buffer_view.byte_stride == 0u ? componentSize(accessor.component_type) : buffer_view.byte_stride;
  const std::size_t offset = buffer_view.byte_offset + accessor.byte_offset + element * stride;
  if (accessor.component_type == 5121) {
    return readScalar<std::uint8_t>(buffer, offset);
  }
  if (accessor.component_type == 5123) {
    return readScalar<std::uint16_t>(buffer, offset);
  }
  if (accessor.component_type == 5125) {
    return readScalar<std::uint32_t>(buffer, offset);
  }
  throw std::runtime_error("Scene asset index component type is unsupported.");
}

Vec2 readVec2(const AssetData &data, const std::size_t accessor_index, const std::size_t element) {
  const Accessor &a = accessor(data, accessor_index);
  return {readFloatComponent(data, a, element, 0u), readFloatComponent(data, a, element, 1u)};
}

Vec3 readVec3(const AssetData &data, const std::size_t accessor_index, const std::size_t element) {
  const Accessor &a = accessor(data, accessor_index);
  return {readFloatComponent(data, a, element, 0u), readFloatComponent(data, a, element, 1u),
          readFloatComponent(data, a, element, 2u)};
}

Vec4 readVec4(const AssetData &data, const std::size_t accessor_index, const std::size_t element) {
  const Accessor &a = accessor(data, accessor_index);
  return {readFloatComponent(data, a, element, 0u), readFloatComponent(data, a, element, 1u),
          readFloatComponent(data, a, element, 2u), readFloatComponent(data, a, element, 3u)};
}

std::size_t materialSlot(const Json *material) {
  return material == nullptr ? 0u : static_cast<std::size_t>(material->numeric()) + 1u;
}

SceneMaterialSlot importedMaterialSlot(const Json *source) {
  SceneMaterialSlot out;
  if (source == nullptr) {
    out.name = "Default";
    const CompiledMaterial compiled = compileMaterialForRendering(out.material, false, out.name);
    out.material = compiled.material;
    out.material.compiled_permutation_key = compiled.permutation_key;
    out.material.compiled_permutation_flags = compiled.permutation_flags;
    out.permutation_key = compiled.permutation_key;
    out.permutation_flags = compiled.permutation_flags;
    out.pipeline_tag = compiled.pipeline_tag;
    return out;
  }
  if (const Json *name = source->find("name")) {
    out.name = std::string(name->text());
  }
  if (const Json *pbr = source->find("pbrMetallicRoughness")) {
    if (const Json *base = pbr->find("baseColorFactor"); base != nullptr && base->array.size() >= 3u) {
      out.material.base_color = srgbToLinear(Srgb{static_cast<float>(base->array[0].numeric(1.0)),
                                                  static_cast<float>(base->array[1].numeric(1.0)),
                                                  static_cast<float>(base->array[2].numeric(1.0))});
      if (base->array.size() >= 4u) {
        out.material.opacity = static_cast<float>(base->array[3].numeric(1.0));
      }
    }
    if (const Json *metallic = pbr->find("metallicFactor")) {
      out.material.metallic = static_cast<float>(metallic->numeric(out.material.metallic));
    }
    if (const Json *roughness = pbr->find("roughnessFactor")) {
      out.material.roughness = static_cast<float>(roughness->numeric(out.material.roughness));
    }
    out.has_base_color_texture = pbr->find("baseColorTexture") != nullptr;
    out.has_metallic_roughness_texture = pbr->find("metallicRoughnessTexture") != nullptr;
  }
  if (const Json *emissive = source->find("emissiveFactor");
      emissive != nullptr && emissive->array.size() >= 3u) {
    const LinearRgb emission =
        srgbToLinear(Srgb{static_cast<float>(emissive->array[0].numeric()),
                          static_cast<float>(emissive->array[1].numeric()),
                          static_cast<float>(emissive->array[2].numeric())});
    out.material.emission_color = EmissionColor{emission.value};
    out.material.emission_strength = std::max(
        {out.material.emission_color.x, out.material.emission_color.y, out.material.emission_color.z});
  }
  out.has_normal_texture = source->find("normalTexture") != nullptr;
  out.has_occlusion_texture = source->find("occlusionTexture") != nullptr;
  out.material.double_sided =
      source->find("doubleSided") != nullptr && source->find("doubleSided")->flag(false);
  if (const Json *alpha = source->find("alphaMode")) {
    const std::string_view mode = alpha->text();
    if (mode == "BLEND") {
      out.material.alpha_mode = MaterialAlphaMode::Blend;
      out.material.depth_write = MaterialDepthWrite::Disabled;
    } else if (mode == "MASK") {
      out.material.alpha_mode = MaterialAlphaMode::Masked;
    } else {
      out.material.alpha_mode = MaterialAlphaMode::Opaque;
    }
  }
  const bool has_textures = out.has_base_color_texture || out.has_metallic_roughness_texture ||
                            out.has_normal_texture || out.has_occlusion_texture;
  const CompiledMaterial compiled =
      compileMaterialForRendering(out.material, has_textures, out.name);
  out.material = compiled.material;
  out.material.compiled_permutation_key = compiled.permutation_key;
  out.material.compiled_permutation_flags = compiled.permutation_flags;
  out.permutation_key = compiled.permutation_key;
  out.permutation_flags = compiled.permutation_flags;
  out.pipeline_tag = compiled.pipeline_tag;
  return out;
}

std::vector<std::uint32_t> readElementIndices(const AssetData &data, const Json &primitive,
                                              const std::size_t vertex_count) {
  const Json *indices_value = primitive.find("indices");
  if (indices_value == nullptr) {
    std::vector<std::uint32_t> indices(vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i) {
      indices[i] = static_cast<std::uint32_t>(i);
    }
    return indices;
  }

  const Accessor &indices_accessor = accessor(data, static_cast<std::size_t>(indices_value->numeric()));
  std::vector<std::uint32_t> indices(indices_accessor.count);
  for (std::size_t i = 0; i < indices.size(); ++i) {
    indices[i] = readIndex(data, indices_accessor, i);
  }
  return indices;
}

std::vector<std::uint32_t> triangulate(const int mode, const std::vector<std::uint32_t> &source) {
  if (mode == 4) {
    if (source.size() % 3u != 0u) {
      throw std::runtime_error("Scene asset triangle primitive has non-triangular index count.");
    }
    return source;
  }
  if (mode == 5) {
    std::vector<std::uint32_t> triangles;
    for (std::size_t i = 0; i + 2u < source.size(); ++i) {
      if (i % 2u == 0u) {
        triangles.insert(triangles.end(), {source[i], source[i + 1u], source[i + 2u]});
      } else {
        triangles.insert(triangles.end(), {source[i + 1u], source[i], source[i + 2u]});
      }
    }
    return triangles;
  }
  if (mode == 6) {
    std::vector<std::uint32_t> triangles;
    for (std::size_t i = 1; i + 1u < source.size(); ++i) {
      triangles.insert(triangles.end(), {source[0], source[i], source[i + 1u]});
    }
    return triangles;
  }
  throw std::runtime_error("Only triangle scene asset primitives can be imported as meshes.");
}

Mat4 nodeMatrix(const Json &node) {
  if (const Json *matrix = node.find("matrix"); matrix != nullptr && matrix->array.size() >= 16u) {
    Mat4 out{};
    for (std::size_t i = 0; i < 16u; ++i) {
      out.m[i] = static_cast<float>(matrix->array[i].numeric());
    }
    return out;
  }
  Mat4 out = identity();
  if (const Json *translation_value = node.find("translation");
      translation_value != nullptr && translation_value->array.size() >= 3u) {
    out = out * translation({static_cast<float>(translation_value->array[0].numeric()),
                             static_cast<float>(translation_value->array[1].numeric()),
                             static_cast<float>(translation_value->array[2].numeric())});
  }
  if (const Json *scale_value = node.find("scale"); scale_value != nullptr && scale_value->array.size() >= 3u) {
    out = out * scale({static_cast<float>(scale_value->array[0].numeric(1.0)),
                       static_cast<float>(scale_value->array[1].numeric(1.0)),
                       static_cast<float>(scale_value->array[2].numeric(1.0))});
  }
  return out;
}

Vec3 transformNormal(const Mat4 &matrix, const Vec3 value) {
  if (const MathResult<Mat3> normal_matrix = normalMatrix(matrix)) {
    const Vec3 transformed = normal_matrix.value * value;
    if (length(transformed) > 0.0001f) {
      return normalize(transformed);
    }
  }
  return normalize(transformVector(matrix, value));
}

CpuMesh meshFromPrimitive(const AssetData &data, const Json &primitive, const Mat4 &matrix,
                          const bool apply_transform, const float unit_scale) {
  const Json &attributes = primitive.at("attributes");
  const Json *position_attribute = attributes.find("POSITION");
  if (position_attribute == nullptr) {
    throw std::runtime_error("Scene asset primitive is missing POSITION.");
  }
  const std::size_t position_accessor = static_cast<std::size_t>(position_attribute->numeric());
  const Json *normal_attribute = attributes.find("NORMAL");
  const Json *texcoord_attribute = attributes.find("TEXCOORD_0");
  const Json *tangent_attribute = attributes.find("TANGENT");

  CpuMesh mesh;
  mesh.vertices.reserve(accessor(data, position_accessor).count);
  for (std::size_t i = 0; i < accessor(data, position_accessor).count; ++i) {
    Vertex vertex;
    vertex.position = readVec3(data, position_accessor, i);
    if (normal_attribute != nullptr) {
      vertex.normal = readVec3(data, static_cast<std::size_t>(normal_attribute->numeric()), i);
    }
    if (texcoord_attribute != nullptr) {
      vertex.uv = readVec2(data, static_cast<std::size_t>(texcoord_attribute->numeric()), i);
    }
    if (tangent_attribute != nullptr) {
      vertex.tangent = readVec4(data, static_cast<std::size_t>(tangent_attribute->numeric()), i);
    }
    if (apply_transform) {
      vertex.position = transformPoint(matrix, vertex.position);
      vertex.normal = transformNormal(matrix, vertex.normal);
      const Vec3 tangent = normalize(transformVector(
          matrix, {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}));
      vertex.tangent = {tangent.x, tangent.y, tangent.z, vertex.tangent.w};
    }
    vertex.position = vertex.position * unit_scale;
    mesh.vertices.push_back(vertex);
  }

  const int mode = static_cast<int>(primitive.find("mode") == nullptr ? 4 : primitive.find("mode")->numeric(4));
  mesh.indices = triangulate(mode, readElementIndices(data, primitive, mesh.vertices.size()));
  return mesh;
}

struct Bounds {
  Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  Vec3 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};
  bool valid = false;
};

void includeInBounds(Bounds &bounds, const Vec3 position) {
  bounds.minimum.x = std::min(bounds.minimum.x, position.x);
  bounds.minimum.y = std::min(bounds.minimum.y, position.y);
  bounds.minimum.z = std::min(bounds.minimum.z, position.z);
  bounds.maximum.x = std::max(bounds.maximum.x, position.x);
  bounds.maximum.y = std::max(bounds.maximum.y, position.y);
  bounds.maximum.z = std::max(bounds.maximum.z, position.z);
  bounds.valid = true;
}

Bounds sceneAssetBounds(const SceneAsset &asset) {
  Bounds bounds;
  for (const SceneMeshChunk &chunk : asset.mesh_chunks) {
    for (const Vertex &vertex : chunk.mesh.vertices) {
      includeInBounds(bounds, vertex.position);
    }
  }
  return bounds;
}

void applyOriginPolicy(SceneAsset &asset, const AssetOriginPolicy policy) {
  if (policy == AssetOriginPolicy::Keep) {
    return;
  }
  const Bounds bounds = sceneAssetBounds(asset);
  if (!bounds.valid) {
    return;
  }
  Vec3 offset{};
  if (policy == AssetOriginPolicy::Center) {
    offset = (bounds.minimum + bounds.maximum) * 0.5f;
  } else if (policy == AssetOriginPolicy::CenterOnGround) {
    offset = {(bounds.minimum.x + bounds.maximum.x) * 0.5f, bounds.minimum.y,
              (bounds.minimum.z + bounds.maximum.z) * 0.5f};
  }
  for (SceneMeshChunk &chunk : asset.mesh_chunks) {
    for (Vertex &vertex : chunk.mesh.vertices) {
      vertex.position = vertex.position - offset;
    }
  }
}

void importNode(const AssetData &data, const Json &node, const Mat4 &parent,
                const SceneAssetImportOptions &options, SceneAsset &asset) {
  const Mat4 world = parent * nodeMatrix(node);
  if (const Json *mesh_index_value = node.find("mesh")) {
    const Json &mesh = data.root.at("meshes").at(static_cast<std::size_t>(mesh_index_value->numeric()));
    const std::string mesh_name(mesh.find("name") == nullptr ? "" : mesh.find("name")->text());
    const Json *node_name = node.find("name");
    for (const Json &primitive : mesh.at("primitives").array) {
      SceneMeshChunk chunk;
      chunk.name = node_name == nullptr || node_name->text().empty()
                       ? mesh_name
                       : std::string(node_name->text()) + "/" + mesh_name;
      chunk.material_slot = materialSlot(primitive.find("material"));
      MeshProcessOptions mesh_options = options.mesh_options;
      if (primitive.at("attributes").find("TANGENT") != nullptr) {
        mesh_options.generate_tangents = false;
      }
      chunk.mesh = prepareMeshForRendering(
          meshFromPrimitive(data, primitive, world, options.apply_node_transforms, options.unit_scale),
          mesh_options, &chunk.diagnostics);
      asset.mesh_chunks.push_back(std::move(chunk));
    }
  }
  if (const Json *children = node.find("children")) {
    for (const Json &child : children->array) {
      importNode(data, data.root.at("nodes").at(static_cast<std::size_t>(child.numeric())), world,
                 options, asset);
    }
  }
}

AssetData loadAssetData(const std::filesystem::path &path) {
  AssetData data;
  data.root = JsonParser(readTextFile(path)).parse();
  const std::filesystem::path base = path.parent_path();

  for (const Json &buffer : data.root.at("buffers").array) {
    data.buffers.push_back(readBinaryFile(base / std::string(buffer.at("uri").text())));
  }
  for (const Json &entry : data.root.at("bufferViews").array) {
    data.views.push_back({integer(entry.find("buffer")), integer(entry.find("byteOffset")),
                          integer(entry.find("byteLength")), integer(entry.find("byteStride"))});
  }
  for (const Json &entry : data.root.at("accessors").array) {
    data.accessors.push_back({integer(entry.find("bufferView")), integer(entry.find("byteOffset")),
                              static_cast<int>(integer(entry.find("componentType"))),
                              integer(entry.find("count")),
                              std::string(entry.at("type").text())});
  }
  return data;
}

} // namespace

SceneAsset importSceneAsset(const std::filesystem::path &path,
                            const SceneAssetImportOptions options) {
  AssetData data = loadAssetData(path);
  SceneAsset asset;
  asset.material_slots.push_back(importedMaterialSlot(nullptr));
  if (const Json *materials = data.root.find("materials")) {
    for (const Json &material : materials->array) {
      asset.material_slots.push_back(importedMaterialSlot(&material));
    }
  }

  const std::size_t scene_index =
      data.root.find("scene") == nullptr ? 0u : static_cast<std::size_t>(data.root.find("scene")->numeric());
  const Json &scene = data.root.at("scenes").at(scene_index);
  for (const Json &node_index : scene.at("nodes").array) {
    importNode(data, data.root.at("nodes").at(static_cast<std::size_t>(node_index.numeric())),
               identity(), options, asset);
  }
  if (asset.mesh_chunks.empty()) {
    throw std::runtime_error("Scene asset contains no renderable mesh primitives.");
  }
  applyOriginPolicy(asset, options.origin_policy);
  return asset;
}

} // namespace aster
