// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/shader/shader_reflection.hpp"

namespace aster {

ShaderReflection reflectShaderVariantBindings(const ShaderVariantKey &variant) {
  ShaderReflection reflection;
  reflection.resources.push_back({.name = "Scene",
                                  .kind = ShaderResourceKind::UniformBuffer,
                                  .binding = 0u,
                                  .count = 1u});
  reflection.resources.push_back({.name = "MaterialParameters",
                                  .kind = ShaderResourceKind::UniformBuffer,
                                  .binding = 1u,
                                  .count = 1u});
  std::uint32_t binding = 2u;
  if ((variant.feature_mask & shaderFeatureFlagBit(ShaderFeatureFlag::Textured)) != 0u) {
    reflection.resources.push_back(
        {.name = "AlbedoTexture", .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if ((variant.feature_mask & shaderFeatureFlagBit(ShaderFeatureFlag::NormalMap)) != 0u) {
    reflection.resources.push_back(
        {.name = "NormalTexture", .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if ((variant.feature_mask & shaderFeatureFlagBit(ShaderFeatureFlag::OrmTexture)) != 0u) {
    reflection.resources.push_back(
        {.name = "OrmTexture", .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if ((variant.feature_mask & shaderFeatureFlagBit(ShaderFeatureFlag::Emissive)) != 0u) {
    reflection.resources.push_back(
        {.name = "EmissiveTexture", .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if ((variant.feature_mask & shaderFeatureFlagBit(ShaderFeatureFlag::Height)) != 0u) {
    reflection.resources.push_back(
        {.name = "HeightTexture", .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if (binding > 2u) {
    reflection.resources.push_back(
        {.name = "MaterialSampler", .kind = ShaderResourceKind::Sampler, .binding = binding});
  }
  return reflection;
}

} // namespace aster
