// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/texture/runtime_texture.hpp"

#include "aster/asset/asset_database.hpp"
#include "aster/math/color.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace aster {
namespace {

constexpr std::array<std::string_view, 10> kRuntimeTextureRoles{
    "albedo", "normal", "orm",      "roughness", "metallic",
    "ao",     "height", "emissive", "wetness",   "opacity"};

std::uint64_t hashString(const std::string_view value) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char byte : value) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::uint8_t toByte(const float value) {
  return static_cast<std::uint8_t>(
      std::clamp(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0l, 255l));
}

std::array<std::uint8_t, 4> fallbackPixel(const std::string_view role) {
  if (role == "normal") {
    return {128u, 128u, 255u, 255u};
  }
  if (role == "orm") {
    return {255u, 178u, 0u, 255u};
  }
  if (role == "roughness") {
    return {178u, 178u, 178u, 255u};
  }
  if (role == "metallic" || role == "height" || role == "emissive" || role == "wetness") {
    return {0u, 0u, 0u, 255u};
  }
  if (role == "ao" || role == "opacity" || role == "albedo") {
    return {255u, 255u, 255u, 255u};
  }
  return {255u, 255u, 255u, 255u};
}

std::array<std::uint8_t, 4> seededPixel(const std::string_view role,
                                        const std::filesystem::path &source_path,
                                        const std::uint32_t x,
                                        const std::uint32_t y) {
  const std::uint64_t seed =
      hashString(role) ^ (hashString(source_path.generic_string()) << 1u) ^
      (static_cast<std::uint64_t>(x) * 0x9e3779b97f4a7c15ull) ^
      (static_cast<std::uint64_t>(y) * 0xbf58476d1ce4e5b9ull);
  const float a = static_cast<float>((seed >> 8u) & 0xffu) / 255.0f;
  const float b = static_cast<float>((seed >> 24u) & 0xffu) / 255.0f;
  const float checker = ((x / 8u + y / 8u) & 1u) == 0u ? 0.92f : 1.08f;
  if (role == "albedo") {
    return {toByte((0.36f + a * 0.42f) * checker), toByte((0.32f + b * 0.32f) * checker),
            toByte((0.28f + (1.0f - a) * 0.28f) * checker), 255u};
  }
  if (role == "normal") {
    return {toByte(0.48f + (a - 0.5f) * 0.18f), toByte(0.52f + (b - 0.5f) * 0.18f), 255u,
            255u};
  }
  if (role == "orm") {
    return {toByte(0.74f + a * 0.22f), toByte(0.34f + b * 0.52f),
            toByte(((seed >> 40u) & 0xffu) > 210u ? 0.68f : 0.04f), 255u};
  }
  if (role == "roughness") {
    return {toByte(0.28f + a * 0.58f), toByte(0.28f + a * 0.58f),
            toByte(0.28f + a * 0.58f), 255u};
  }
  if (role == "metallic") {
    return {toByte(((seed >> 32u) & 0xffu) > 220u ? 0.8f : 0.0f), 0u, 0u, 255u};
  }
  if (role == "ao") {
    return {toByte(0.68f + a * 0.30f), toByte(0.68f + a * 0.30f),
            toByte(0.68f + a * 0.30f), 255u};
  }
  if (role == "height") {
    return {toByte(0.35f + a * 0.38f), toByte(0.35f + a * 0.38f),
            toByte(0.35f + a * 0.38f), 255u};
  }
  if (role == "emissive") {
    const float glow = ((seed >> 17u) & 0xffu) > 224u ? 0.9f : 0.0f;
    return {toByte(glow), toByte(glow * 0.55f), toByte(glow * 0.20f), 255u};
  }
  if (role == "wetness") {
    return {toByte(a > 0.58f ? (a - 0.58f) * 2.3f : 0.0f), 0u, 0u, 255u};
  }
  if (role == "opacity") {
    return {255u, 255u, 255u, 255u};
  }
  return fallbackPixel(role);
}

void appendMipPayload(RuntimeTexture &texture, const TextureAssetMetadata &metadata) {
  const std::uint32_t resident_width = std::clamp(metadata.width == 0u ? 1u : metadata.width, 1u, 64u);
  const std::uint32_t resident_height =
      std::clamp(metadata.height == 0u ? 1u : metadata.height, 1u, 64u);
  texture.width = resident_width;
  texture.height = resident_height;
  texture.rgba8.resize(static_cast<std::size_t>(resident_width) * resident_height * 4u);
  for (std::uint32_t y = 0u; y < resident_height; ++y) {
    for (std::uint32_t x = 0u; x < resident_width; ++x) {
      const std::array<std::uint8_t, 4> pixel =
          seededPixel(texture.role, texture.source_path, x, y);
      const std::size_t offset =
          (static_cast<std::size_t>(y) * resident_width + static_cast<std::size_t>(x)) * 4u;
      texture.rgba8[offset + 0u] = pixel[0];
      texture.rgba8[offset + 1u] = pixel[1];
      texture.rgba8[offset + 2u] = pixel[2];
      texture.rgba8[offset + 3u] = pixel[3];
    }
  }
  texture.mips = metadata.mips;
  if (texture.mips.empty()) {
    texture.mips.push_back({.width = resident_width, .height = resident_height});
  }
}

float wrapCoordinate(float value, const rhi::AddressMode mode) {
  switch (mode) {
  case rhi::AddressMode::Repeat:
    value = value - std::floor(value);
    return value < 0.0f ? value + 1.0f : value;
  case rhi::AddressMode::MirroredRepeat: {
    const float wrapped = value - std::floor(value * 0.5f) * 2.0f;
    return wrapped <= 1.0f ? wrapped : 2.0f - wrapped;
  }
  case rhi::AddressMode::ClampToEdge:
    return std::clamp(value, 0.0f, 1.0f);
  }
  return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

const RuntimeTexture *RuntimeTextureSet::find(const std::string_view role) const {
  const auto it = textures.find(std::string(canonicalMaterialTextureRole(role)));
  return it == textures.end() ? nullptr : &it->second;
}

bool RuntimeTextureSet::contains(const std::string_view role) const {
  return find(role) != nullptr;
}

void MaterialResourceLibrary::clear() {
  resources_.clear();
}

bool MaterialResourceLibrary::addMaterialAsset(const MaterialAsset &asset,
                                               const std::filesystem::path &asset_root,
                                               const TextureImportOptions options) {
  if (asset.id.empty()) {
    return false;
  }
  const std::filesystem::path root =
      asset_root.empty() ? asset.source_path.parent_path() : asset_root;
  auto resource = std::make_unique<MaterialRuntimeResource>();
  resource->asset_id = asset.id;
  resource->compiled = compileMaterialAssetForRendering(asset);
  resource->parameters = materialParameterBlockForAsset(asset);
  resource->fallback_material = resolveMaterialAssetFallback(asset);
  const RuntimeTextureSet texture_set = runtimeTextureSetForMaterial(asset, root, options);
  if (options.require_existing_files) {
    for (const auto &[role, slot] : asset.textures) {
      if (!slot.required) {
        continue;
      }
      const RuntimeTexture *texture = texture_set.find(role);
      if (texture == nullptr || !texture->valid || texture->fallback) {
        return false;
      }
    }
  }
  resource->texture_set = texture_set;
  resource->receives_shadows = asset.receives_shadows;
  const std::string key = resource->asset_id;
  resources_[key] = std::move(resource);
  return true;
}

bool MaterialResourceLibrary::addCookedMaterialAsset(const CookedMaterialAsset &asset,
                                                     const TextureImportOptions options) {
  if (asset.asset.id.empty()) {
    return false;
  }
  return addMaterialAsset(asset.asset, {}, options);
}

bool MaterialResourceLibrary::addCookedMaterialAssetFile(
    const std::filesystem::path &material_bin_path, const TextureImportOptions options) {
  return addCookedMaterialAsset(loadCookedMaterialAsset(material_bin_path), options);
}

const MaterialRuntimeResource *MaterialResourceLibrary::find(const std::string_view asset_id) const {
  if (asset_id.empty()) {
    return nullptr;
  }
  const auto it = resources_.find(std::string(asset_id));
  return it == resources_.end() ? nullptr : it->second.get();
}

const MaterialRuntimeResource *MaterialResourceLibrary::findForMaterialIds(
    const std::string_view render_object_material_asset_id,
    const std::string_view material_asset_id) const {
  if (const MaterialRuntimeResource *resource = find(render_object_material_asset_id)) {
    return resource;
  }
  return find(material_asset_id);
}

std::size_t MaterialResourceLibrary::size() const noexcept {
  return resources_.size();
}

std::span<const std::string_view> materialRuntimeTextureRoles() {
  return {kRuntimeTextureRoles.data(), kRuntimeTextureRoles.size()};
}

std::string_view canonicalMaterialTextureRole(const std::string_view role) {
  if (role == "base_color" || role == "baseColor" || role == "diffuse") {
    return "albedo";
  }
  if (role == "ambient_occlusion" || role == "occlusion") {
    return "ao";
  }
  if (role == "alpha") {
    return "opacity";
  }
  if (role == "metallic_roughness") {
    return "orm";
  }
  return role;
}

TextureSamplerDesc defaultTextureSamplerDesc(const TextureKind kind) {
  TextureSamplerDesc desc;
  desc.address_u = rhi::AddressMode::Repeat;
  desc.address_v = rhi::AddressMode::Repeat;
  desc.address_w = rhi::AddressMode::ClampToEdge;
  desc.max_anisotropy =
      kind == TextureKind::Albedo || kind == TextureKind::Normal || kind == TextureKind::ORM
          ? 8.0f
          : 1.0f;
  return desc;
}

RuntimeTexture makeRuntimeFallbackTexture(const std::string_view role) {
  RuntimeTexture texture;
  texture.role = std::string(canonicalMaterialTextureRole(role));
  texture.kind = textureKindForRole(texture.role);
  texture.color_space = defaultTextureColorSpace(texture.kind);
  texture.normal_convention = TextureNormalConvention::OpenGlYUp;
  texture.sampler = defaultTextureSamplerDesc(texture.kind);
  texture.width = 1u;
  texture.height = 1u;
  texture.mips = {{1u, 1u, 0u, 4u}};
  const std::array<std::uint8_t, 4> pixel = fallbackPixel(texture.role);
  texture.rgba8.assign(pixel.begin(), pixel.end());
  texture.valid = true;
  texture.fallback = true;
  return texture;
}

RuntimeTexture runtimeTextureForSlot(const std::string_view role, const MaterialTextureSlot &slot,
                                     const std::filesystem::path &asset_root,
                                     const TextureImportOptions options) {
  const std::string canonical_role(canonicalMaterialTextureRole(role));
  std::filesystem::path path = slot.uri;
  if (!path.is_absolute()) {
    path = asset_root / path;
  }
  TextureImportOptions inspect_options = options;
  if (!slot.required) {
    inspect_options.require_existing_files = false;
  }
  const TextureKind kind = textureKindForRole(canonical_role);
  const TextureAssetMetadata metadata = inspectTextureAsset(path, kind, inspect_options);
  if (!metadata.valid) {
    return makeRuntimeFallbackTexture(canonical_role);
  }

  RuntimeTexture texture;
  texture.role = canonical_role;
  texture.source_path = path;
  texture.kind = kind;
  texture.color_space = slot.srgb ? TextureColorSpace::SRGB : metadata.color_space;
  texture.compression = metadata.compression;
  texture.normal_convention = TextureNormalConvention::OpenGlYUp;
  texture.sampler = defaultTextureSamplerDesc(kind);
  texture.valid = true;
  texture.fallback = !std::filesystem::exists(path);
  if (texture.fallback) {
    const RuntimeTexture fallback = makeRuntimeFallbackTexture(canonical_role);
    texture.width = fallback.width;
    texture.height = fallback.height;
    texture.mips = fallback.mips;
    texture.rgba8 = fallback.rgba8;
  } else {
    appendMipPayload(texture, metadata);
  }
  return texture;
}

RuntimeTextureSet runtimeTextureSetForMaterial(const MaterialAsset &asset,
                                               const std::filesystem::path &asset_root,
                                               const TextureImportOptions options) {
  RuntimeTextureSet set;
  for (const std::string_view role : materialRuntimeTextureRoles()) {
    set.textures.emplace(std::string(role), makeRuntimeFallbackTexture(role));
  }
  const std::filesystem::path root =
      asset_root.empty() ? asset.source_path.parent_path() : asset_root;
  for (const auto &[role, slot] : asset.textures) {
    const std::string canonical_role(canonicalMaterialTextureRole(role));
    set.textures[canonical_role] = runtimeTextureForSlot(canonical_role, slot, root, options);
  }
  return set;
}

Vec4 sampleRuntimeTexture(const RuntimeTexture &texture, Vec2 uv) {
  if (texture.rgba8.empty() || texture.width == 0u || texture.height == 0u) {
    const RuntimeTexture fallback = makeRuntimeFallbackTexture(texture.role);
    return sampleRuntimeTexture(fallback, uv);
  }
  const float u = wrapCoordinate(uv.x, texture.sampler.address_u);
  const float v = wrapCoordinate(uv.y, texture.sampler.address_v);
  const std::uint32_t x =
      std::min(static_cast<std::uint32_t>(u * static_cast<float>(texture.width)), texture.width - 1u);
  const std::uint32_t y =
      std::min(static_cast<std::uint32_t>(v * static_cast<float>(texture.height)), texture.height - 1u);
  const std::size_t offset =
      (static_cast<std::size_t>(y) * texture.width + static_cast<std::size_t>(x)) * 4u;
  if (offset + 3u >= texture.rgba8.size()) {
    return {1.0f, 1.0f, 1.0f, 1.0f};
  }
  return {static_cast<float>(texture.rgba8[offset + 0u]) / 255.0f,
          static_cast<float>(texture.rgba8[offset + 1u]) / 255.0f,
          static_cast<float>(texture.rgba8[offset + 2u]) / 255.0f,
          static_cast<float>(texture.rgba8[offset + 3u]) / 255.0f};
}

Vec3 sampleRuntimeTextureRgb(const RuntimeTexture &texture, const Vec2 uv) {
  const Vec4 rgba = sampleRuntimeTexture(texture, uv);
  if (texture.color_space == TextureColorSpace::SRGB) {
    return srgbToLinear(Srgb{rgba.x, rgba.y, rgba.z}).value;
  }
  return {rgba.x, rgba.y, rgba.z};
}

float sampleRuntimeTextureChannel(const RuntimeTexture &texture, const Vec2 uv,
                                  const std::uint32_t channel) {
  const Vec4 rgba = sampleRuntimeTexture(texture, uv);
  return rgba[std::min(channel, 3u)];
}

} // namespace aster
