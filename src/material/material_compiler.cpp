// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/material/material_compiler.hpp"

#include "aster/shader/shader_variant.hpp"

namespace aster {

MaterialBindingLayout bindingLayoutForMaterial(const MaterialAsset &asset) {
  MaterialBindingLayout layout;
  layout.bindings.push_back({.name = "MaterialParameters",
                             .kind = ShaderResourceKind::UniformBuffer,
                             .binding = 0u});
  std::uint32_t binding = 1u;
  for (const auto &[role, slot] : asset.textures) {
    (void)slot;
    layout.bindings.push_back(
        {.name = role, .kind = ShaderResourceKind::Texture, .binding = binding++});
  }
  if (!asset.textures.empty()) {
    layout.bindings.push_back(
        {.name = "MaterialSampler", .kind = ShaderResourceKind::Sampler, .binding = binding});
  }
  return layout;
}

CompiledMaterialAsset compileMaterialAssetForRendering(const MaterialAsset &asset) {
  CompiledMaterialAsset compiled;
  compiled.asset = asset;
  compiled.diagnostics = validateMaterialAsset(asset);
  compiled.variant = shaderVariantKeyForMaterial(asset);
  compiled.binding_layout = bindingLayoutForMaterial(asset);

  Material fallback = resolveMaterialAssetFallback(asset);
  compiled.fallback_material =
      compileMaterialForRendering(fallback, !asset.textures.empty(), asset.id);
  return compiled;
}

} // namespace aster
