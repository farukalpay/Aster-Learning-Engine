// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_variant.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aster {
namespace {

void appendByte(std::uint64_t &hash, const std::uint8_t byte) {
  hash ^= byte;
  hash *= 1099511628211ull;
}

void appendU64(std::uint64_t &hash, const std::uint64_t value) {
  for (int byte = 0; byte < 8; ++byte) {
    appendByte(hash, static_cast<std::uint8_t>(value >> (byte * 8)));
  }
}

void appendText(std::uint64_t &hash, const std::string_view text) {
  for (const char c : text) {
    appendByte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
  }
  appendU64(hash, text.size());
}

void appendFeature(ShaderPermutationSelection &selection, const ShaderVariantKey &variant,
                   const ShaderFeatureFlag flag, const char *dimension_name) {
  selection.set(dimension_name,
                (variant.feature_mask & shaderFeatureFlagBit(flag)) != 0u ? 1 : 0);
}

std::string qualityName(const std::uint32_t quality_level) {
  switch (quality_level) {
  case 0u:
    return "preview";
  case 1u:
    return "balanced";
  case 2u:
    return "production";
  case 3u:
    return "cinematic";
  default:
    return "custom-" + std::to_string(quality_level);
  }
}

} // namespace

ShaderPermutationDimension ShaderPermutationDimension::boolean(std::string name,
                                                               std::string define_name,
                                                               const bool default_value) {
  return {.name = std::move(name),
          .define_name = std::move(define_name),
          .kind = ShaderPermutationDimensionKind::Boolean,
          .first_value = 0,
          .value_count = 2u,
          .sparse_values = {},
          .default_value = default_value ? 1 : 0};
}

ShaderPermutationDimension ShaderPermutationDimension::denseInt(
    std::string name, std::string define_name, const std::int32_t first,
    const std::uint32_t count, const std::int32_t default_value) {
  return {.name = std::move(name),
          .define_name = std::move(define_name),
          .kind = ShaderPermutationDimensionKind::DenseInt,
          .first_value = first,
          .value_count = count,
          .sparse_values = {},
          .default_value = default_value};
}

ShaderPermutationDimension ShaderPermutationDimension::sparseInt(
    std::string name, std::string define_name, std::vector<std::int32_t> values,
    const std::int32_t default_value) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return {.name = std::move(name),
          .define_name = std::move(define_name),
          .kind = ShaderPermutationDimensionKind::SparseInt,
          .first_value = 0,
          .value_count = static_cast<std::uint32_t>(values.size()),
          .sparse_values = std::move(values),
          .default_value = default_value};
}

bool ShaderPermutationDimension::contains(const std::int32_t value) const {
  switch (kind) {
  case ShaderPermutationDimensionKind::Boolean:
    return value == 0 || value == 1;
  case ShaderPermutationDimensionKind::DenseInt:
    return value_count > 0u && value >= first_value &&
           value < first_value + static_cast<std::int32_t>(value_count);
  case ShaderPermutationDimensionKind::SparseInt:
    return std::binary_search(sparse_values.begin(), sparse_values.end(), value);
  }
  return false;
}

std::uint32_t ShaderPermutationDimension::valueId(const std::int32_t value) const {
  if (!contains(value)) {
    throw std::out_of_range("shader permutation dimension value is outside its domain");
  }
  switch (kind) {
  case ShaderPermutationDimensionKind::Boolean:
    return value == 0 ? 0u : 1u;
  case ShaderPermutationDimensionKind::DenseInt:
    return static_cast<std::uint32_t>(value - first_value);
  case ShaderPermutationDimensionKind::SparseInt: {
    const auto found = std::lower_bound(sparse_values.begin(), sparse_values.end(), value);
    return static_cast<std::uint32_t>(std::distance(sparse_values.begin(), found));
  }
  }
  return 0u;
}

std::uint32_t ShaderPermutationDimension::permutationCount() const {
  return kind == ShaderPermutationDimensionKind::SparseInt
             ? static_cast<std::uint32_t>(sparse_values.size())
             : value_count;
}

std::string ShaderPermutationDimension::defineValue(const std::int32_t value) const {
  return std::to_string(value);
}

ShaderPermutationSelection &ShaderPermutationSelection::set(std::string name,
                                                            const std::int32_t value) {
  values[std::move(name)] = value;
  return *this;
}

std::int32_t
ShaderPermutationSelection::valueFor(const ShaderPermutationDimension &dimension) const {
  const auto found = values.find(dimension.name);
  return found == values.end() ? dimension.default_value : found->second;
}

ShaderPermutationSpace &ShaderPermutationSpace::add(ShaderPermutationDimension dimension) {
  dimensions_.push_back(std::move(dimension));
  return *this;
}

const ShaderPermutationDimension *ShaderPermutationSpace::find(const std::string_view name) const {
  const auto found = std::find_if(dimensions_.begin(), dimensions_.end(),
                                  [name](const ShaderPermutationDimension &dimension) {
                                    return dimension.name == name;
                                  });
  return found == dimensions_.end() ? nullptr : &*found;
}

const std::vector<ShaderPermutationDimension> &ShaderPermutationSpace::dimensions() const noexcept {
  return dimensions_;
}

std::vector<std::string>
ShaderPermutationSpace::validate(const ShaderPermutationSelection &selection) const {
  std::vector<std::string> diagnostics;
  for (const ShaderPermutationDimension &dimension : dimensions_) {
    if (dimension.name.empty()) {
      diagnostics.push_back("shader permutation dimension has an empty name");
    }
    if (dimension.define_name.empty()) {
      diagnostics.push_back("shader permutation dimension '" + dimension.name +
                            "' has an empty define name");
    }
    if (dimension.permutationCount() == 0u) {
      diagnostics.push_back("shader permutation dimension '" + dimension.name +
                            "' has no values");
    }
    const std::int32_t value = selection.valueFor(dimension);
    if (!dimension.contains(value)) {
      diagnostics.push_back("shader permutation dimension '" + dimension.name +
                            "' rejects value " + std::to_string(value));
    }
  }
  for (const auto &[name, value] : selection.values) {
    (void)value;
    if (find(name) == nullptr) {
      diagnostics.push_back("shader permutation selection contains unknown dimension '" + name +
                            "'");
    }
  }
  return diagnostics;
}

std::uint64_t
ShaderPermutationSpace::permutationId(const ShaderPermutationSelection &selection) const {
  std::uint64_t id = 0u;
  std::uint64_t stride = 1u;
  for (const ShaderPermutationDimension &dimension : dimensions_) {
    const std::uint32_t count = std::max(dimension.permutationCount(), 1u);
    const std::uint32_t value_id = dimension.valueId(selection.valueFor(dimension));
    id += stride * value_id;
    stride *= count;
  }
  return id;
}

std::vector<ShaderPermutationDefine>
ShaderPermutationSpace::defines(const ShaderPermutationSelection &selection) const {
  std::vector<ShaderPermutationDefine> defines;
  defines.reserve(dimensions_.size());
  for (const ShaderPermutationDimension &dimension : dimensions_) {
    const std::int32_t value = selection.valueFor(dimension);
    if (dimension.contains(value)) {
      defines.push_back({.name = dimension.define_name, .value = dimension.defineValue(value)});
    }
  }
  std::sort(defines.begin(), defines.end(), [](const ShaderPermutationDefine &lhs,
                                               const ShaderPermutationDefine &rhs) {
    return lhs.name < rhs.name;
  });
  return defines;
}

StableShaderKey ShaderPermutationSpace::stableKey(const ShaderPermutationSelection &selection,
                                                  const std::string_view backend,
                                                  const std::string_view quality,
                                                  const std::string_view material_domain,
                                                  const std::string_view material_id) const {
  StableShaderKey key;
  key.backend = std::string(backend);
  key.quality = std::string(quality);
  key.material_domain = std::string(material_domain);
  key.material_id = std::string(material_id);
  key.permutation_id = permutationId(selection);
  key.defines = defines(selection);

  std::uint64_t hash = 1469598103934665603ull;
  appendText(hash, "aster.shader.stable-key.v1");
  appendText(hash, key.backend);
  appendText(hash, key.quality);
  appendText(hash, key.material_domain);
  appendText(hash, key.material_id);
  appendU64(hash, key.permutation_id);
  for (const ShaderPermutationDefine &define : key.defines) {
    appendText(hash, define.name);
    appendText(hash, define.value);
  }
  key.hash = hash;
  key.tag = key.material_domain + "." + key.backend + "." + key.quality + ".p" +
            std::to_string(key.permutation_id) + "." + shaderStableHex(key.hash);
  return key;
}

std::uint64_t stableShaderHash(const std::string_view text) {
  return stableShaderHashAppend(1469598103934665603ull, text);
}

std::uint64_t stableShaderHashAppend(std::uint64_t hash, const std::string_view text) {
  appendText(hash, text);
  return hash;
}

std::uint64_t stableShaderHashAppend(std::uint64_t hash, const std::uint64_t value) {
  appendU64(hash, value);
  return hash;
}

std::string shaderStableHex(const std::uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

ShaderPermutationSpace makeMaterialShaderPermutationSpace() {
  ShaderPermutationSpace space;
  space.add(ShaderPermutationDimension::sparseInt("backend", "ASTER_SHADER_BACKEND",
                                                  {0, 1, 2}, 0));
  space.add(ShaderPermutationDimension::denseInt("quality", "ASTER_SHADER_QUALITY", 0, 4u, 2));
  space.add(ShaderPermutationDimension::denseInt("shading_model", "ASTER_SHADING_MODEL", 0, 3u,
                                                 static_cast<std::int32_t>(
                                                     MaterialShadingModel::LitPBR)));
  space.add(ShaderPermutationDimension::denseInt("blend_mode", "ASTER_BLEND_MODE", 0, 3u,
                                                 static_cast<std::int32_t>(
                                                     MaterialBlendMode::Opaque)));
  space.add(ShaderPermutationDimension::boolean("feature_textured", "ASTER_FEATURE_TEXTURED"));
  space.add(ShaderPermutationDimension::boolean("feature_normal_map", "ASTER_FEATURE_NORMAL_MAP"));
  space.add(ShaderPermutationDimension::boolean("feature_orm", "ASTER_FEATURE_ORM_TEXTURE"));
  space.add(ShaderPermutationDimension::boolean("feature_emissive", "ASTER_FEATURE_EMISSIVE"));
  space.add(ShaderPermutationDimension::boolean("feature_height", "ASTER_FEATURE_HEIGHT"));
  space.add(ShaderPermutationDimension::boolean("feature_parallax", "ASTER_FEATURE_PARALLAX"));
  space.add(ShaderPermutationDimension::boolean("feature_triplanar", "ASTER_FEATURE_TRIPLANAR"));
  space.add(ShaderPermutationDimension::boolean("feature_decals", "ASTER_FEATURE_DECAL_RECEIVER"));
  space.add(ShaderPermutationDimension::boolean("feature_fog", "ASTER_FEATURE_FOG", true));
  space.add(ShaderPermutationDimension::boolean("feature_shadow", "ASTER_FEATURE_SHADOW", true));
  space.add(ShaderPermutationDimension::boolean("feature_alpha_clip", "ASTER_FEATURE_ALPHA_CLIP"));
  space.add(ShaderPermutationDimension::boolean("feature_alpha_blend", "ASTER_FEATURE_ALPHA_BLEND"));
  space.add(ShaderPermutationDimension::boolean("feature_double_sided",
                                                "ASTER_FEATURE_DOUBLE_SIDED"));
  space.add(ShaderPermutationDimension::boolean("feature_instancing", "ASTER_FEATURE_INSTANCING",
                                                true));
  return space;
}

ShaderPermutationSelection shaderPermutationSelectionForVariant(
    const ShaderVariantKey &variant, const std::uint32_t backend_id,
    const std::uint32_t quality_level) {
  ShaderPermutationSelection selection;
  selection.set("backend", static_cast<std::int32_t>(backend_id));
  selection.set("quality", static_cast<std::int32_t>(quality_level));
  selection.set("shading_model", static_cast<std::int32_t>(variant.shading_model));
  selection.set("blend_mode", static_cast<std::int32_t>(variant.blend_mode));
  appendFeature(selection, variant, ShaderFeatureFlag::Textured, "feature_textured");
  appendFeature(selection, variant, ShaderFeatureFlag::NormalMap, "feature_normal_map");
  appendFeature(selection, variant, ShaderFeatureFlag::OrmTexture, "feature_orm");
  appendFeature(selection, variant, ShaderFeatureFlag::Emissive, "feature_emissive");
  appendFeature(selection, variant, ShaderFeatureFlag::Height, "feature_height");
  appendFeature(selection, variant, ShaderFeatureFlag::Parallax, "feature_parallax");
  appendFeature(selection, variant, ShaderFeatureFlag::Triplanar, "feature_triplanar");
  appendFeature(selection, variant, ShaderFeatureFlag::DecalReceiver, "feature_decals");
  appendFeature(selection, variant, ShaderFeatureFlag::Fog, "feature_fog");
  appendFeature(selection, variant, ShaderFeatureFlag::Shadow, "feature_shadow");
  appendFeature(selection, variant, ShaderFeatureFlag::AlphaClip, "feature_alpha_clip");
  appendFeature(selection, variant, ShaderFeatureFlag::AlphaBlend, "feature_alpha_blend");
  appendFeature(selection, variant, ShaderFeatureFlag::DoubleSided, "feature_double_sided");
  appendFeature(selection, variant, ShaderFeatureFlag::Instancing, "feature_instancing");
  return selection;
}

StableShaderKey stableShaderKeyForVariant(
    const ShaderVariantKey &variant, const std::string_view backend,
    const std::uint32_t backend_id, const std::uint32_t quality_level,
    const std::string_view material_domain, const std::string_view material_id) {
  const ShaderPermutationSpace space = makeMaterialShaderPermutationSpace();
  ShaderPermutationSelection selection =
      shaderPermutationSelectionForVariant(variant, backend_id, quality_level);
  StableShaderKey key =
      space.stableKey(selection, backend, qualityName(quality_level), material_domain, material_id);
  if (!variant.tag.empty()) {
    key.tag = std::string(material_domain) + "." + std::string(backend) + "." +
              qualityName(quality_level) + "." + variant.tag + "." + shaderStableHex(key.hash);
  }
  return key;
}

StableShaderKey stableShaderKeyForMaterial(
    const MaterialAsset &asset, const std::string_view backend, const std::uint32_t backend_id,
    const std::uint32_t quality_level, const std::string_view material_domain) {
  return stableShaderKeyForVariant(shaderVariantKeyForMaterial(asset), backend, backend_id,
                                   quality_level, material_domain, asset.id);
}

ShaderVariantKey shaderVariantKeyForMaterial(const MaterialAsset &asset) {
  ShaderVariantKey key;
  key.shading_model = asset.shading_model;
  key.blend_mode = asset.blend_mode;
  key.feature_mask = materialFeatureMask(materialFeatureSet(asset));
  key.tag = shaderVariantTag(key);
  key.stable_hash = stableShaderKeyForVariant(key).hash;
  return key;
}

std::string shaderVariantTag(const ShaderVariantKey &key) {
  std::ostringstream out;
  out << materialShadingModelName(key.shading_model) << "."
      << materialBlendModeName(key.blend_mode);
  const auto append = [&](const ShaderFeatureFlag flag, const char *name) {
    if ((key.feature_mask & shaderFeatureFlagBit(flag)) != 0u) {
      out << "." << name;
    }
  };
  append(ShaderFeatureFlag::Textured, "textured");
  append(ShaderFeatureFlag::NormalMap, "normal");
  append(ShaderFeatureFlag::OrmTexture, "orm");
  append(ShaderFeatureFlag::Emissive, "emissive");
  append(ShaderFeatureFlag::Height, "height");
  append(ShaderFeatureFlag::Parallax, "parallax");
  append(ShaderFeatureFlag::Triplanar, "triplanar");
  append(ShaderFeatureFlag::DecalReceiver, "decals");
  append(ShaderFeatureFlag::Fog, "fog");
  append(ShaderFeatureFlag::Shadow, "shadow");
  append(ShaderFeatureFlag::AlphaClip, "alpha-clip");
  append(ShaderFeatureFlag::AlphaBlend, "alpha-blend");
  append(ShaderFeatureFlag::DoubleSided, "double-sided");
  append(ShaderFeatureFlag::Instancing, "instancing");
  return out.str();
}

} // namespace aster
