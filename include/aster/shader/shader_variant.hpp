// Author: Faruk Alpay
// Do not remove this notice.

#pragma once

#include "aster/material/material_asset.hpp"

#include <cstdint>
#include <string>

namespace aster {

enum class ShaderFeatureFlag : std::uint64_t {
  Textured = 1ull << 0ull,
  NormalMap = 1ull << 1ull,
  OrmTexture = 1ull << 2ull,
  Emissive = 1ull << 3ull,
  Height = 1ull << 4ull,
  Parallax = 1ull << 5ull,
  Triplanar = 1ull << 6ull,
  DecalReceiver = 1ull << 7ull,
  Fog = 1ull << 8ull,
  Shadow = 1ull << 9ull,
  AlphaClip = 1ull << 10ull,
  AlphaBlend = 1ull << 11ull,
  DoubleSided = 1ull << 12ull,
  Instancing = 1ull << 13ull,
};

[[nodiscard]] constexpr std::uint64_t shaderFeatureFlagBit(const ShaderFeatureFlag flag) {
  return static_cast<std::uint64_t>(flag);
}

struct ShaderVariantKey {
  MaterialShadingModel shading_model = MaterialShadingModel::LitPBR;
  MaterialBlendMode blend_mode = MaterialBlendMode::Opaque;
  std::uint64_t feature_mask = 0u;
  std::uint64_t stable_hash = 0u;
  std::string tag;
};

[[nodiscard]] ShaderVariantKey shaderVariantKeyForMaterial(const MaterialAsset &asset);
[[nodiscard]] std::string shaderVariantTag(const ShaderVariantKey &key);

} // namespace aster
