// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_compiler.hpp"
#include "aster/material/material_parameter_block.hpp"
#include "aster/math/vec.hpp"
#include "aster/rhi/sampler.hpp"
#include "aster/scene/scene.hpp"
#include "aster/texture/texture_importer.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aster {

enum class TextureNormalConvention {
  OpenGlYUp,
  DirectXInvertedY,
};

struct TextureSamplerDesc {
  rhi::FilterMode min_filter = rhi::FilterMode::Linear;
  rhi::FilterMode mag_filter = rhi::FilterMode::Linear;
  rhi::FilterMode mip_filter = rhi::FilterMode::Linear;
  rhi::AddressMode address_u = rhi::AddressMode::Repeat;
  rhi::AddressMode address_v = rhi::AddressMode::Repeat;
  rhi::AddressMode address_w = rhi::AddressMode::ClampToEdge;
  float min_lod = 0.0f;
  float max_lod = 1000.0f;
  float max_anisotropy = 1.0f;
};

struct RuntimeTexture {
  std::string role;
  std::filesystem::path source_path;
  TextureKind kind = TextureKind::Unknown;
  TextureColorSpace color_space = TextureColorSpace::Linear;
  TextureCompression compression = TextureCompression::None;
  TextureNormalConvention normal_convention = TextureNormalConvention::OpenGlYUp;
  TextureSamplerDesc sampler{};
  std::uint32_t width = 1u;
  std::uint32_t height = 1u;
  std::vector<TextureMipInfo> mips;
  std::vector<std::uint8_t> rgba8;
  bool valid = false;
  bool fallback = true;
};

struct RuntimeTextureSet {
  std::map<std::string, RuntimeTexture> textures;

  [[nodiscard]] const RuntimeTexture *find(std::string_view role) const;
  [[nodiscard]] bool contains(std::string_view role) const;
};

struct MaterialRuntimeResource {
  std::string asset_id;
  CompiledMaterialAsset compiled;
  MaterialParameterBlock parameters;
  Material fallback_material;
  RuntimeTextureSet texture_set;
  bool receives_shadows = true;
};

class MaterialResourceLibrary {
public:
  void clear();
  bool addMaterialAsset(const MaterialAsset &asset, const std::filesystem::path &asset_root = {},
                        TextureImportOptions options = {});

  [[nodiscard]] const MaterialRuntimeResource *find(std::string_view asset_id) const;
  [[nodiscard]] const MaterialRuntimeResource *findForMaterialIds(
      std::string_view render_object_material_asset_id,
      std::string_view material_asset_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::map<std::string, std::unique_ptr<MaterialRuntimeResource>> resources_;
};

[[nodiscard]] std::span<const std::string_view> materialRuntimeTextureRoles();
[[nodiscard]] std::string_view canonicalMaterialTextureRole(std::string_view role);
[[nodiscard]] TextureSamplerDesc defaultTextureSamplerDesc(TextureKind kind);
[[nodiscard]] RuntimeTexture makeRuntimeFallbackTexture(std::string_view role);
[[nodiscard]] RuntimeTexture runtimeTextureForSlot(std::string_view role,
                                                   const MaterialTextureSlot &slot,
                                                   const std::filesystem::path &asset_root,
                                                   TextureImportOptions options = {});
[[nodiscard]] RuntimeTextureSet runtimeTextureSetForMaterial(
    const MaterialAsset &asset, const std::filesystem::path &asset_root = {},
    TextureImportOptions options = {});
[[nodiscard]] Vec4 sampleRuntimeTexture(const RuntimeTexture &texture, Vec2 uv);
[[nodiscard]] Vec3 sampleRuntimeTextureRgb(const RuntimeTexture &texture, Vec2 uv);
[[nodiscard]] float sampleRuntimeTextureChannel(const RuntimeTexture &texture, Vec2 uv,
                                                std::uint32_t channel);

} // namespace aster
