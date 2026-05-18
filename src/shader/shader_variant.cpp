// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_variant.hpp"

#include <sstream>

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

} // namespace

ShaderVariantKey shaderVariantKeyForMaterial(const MaterialAsset &asset) {
  ShaderVariantKey key;
  key.shading_model = asset.shading_model;
  key.blend_mode = asset.blend_mode;
  key.feature_mask = materialFeatureMask(materialFeatureSet(asset));
  std::uint64_t hash = 1469598103934665603ull;
  appendU64(hash, static_cast<std::uint64_t>(key.shading_model));
  appendU64(hash, static_cast<std::uint64_t>(key.blend_mode));
  appendU64(hash, key.feature_mask);
  key.stable_hash = hash;
  key.tag = shaderVariantTag(key);
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
